#include "engine/Track.h"

#include <cmath>
#include <cstdint>
#include <cstring>

Track::Track()
{
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new SineVoice());

    synth.addSound (new SineSound());
}

void Track::prepare (double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = blockSize;

    synth.setCurrentPlaybackSampleRate (sampleRate);
    clip.reserve (20000);
    passStarts.reserve (4096);
    trackBuffer.setSize (2, blockSize, false, false, true);

    const juce::ScopedLock sl (pluginLock);
    if (plugin != nullptr)
    {
        plugin->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        plugin->prepareToPlay (sampleRate, blockSize);
    }
}

void Track::setPlugin (std::unique_ptr<juce::AudioPluginInstance> newPlugin)
{
    if (newPlugin != nullptr)
    {
        newPlugin->setPlayConfigDetails (0, 2, currentSampleRate, currentBlockSize);
        newPlugin->prepareToPlay (currentSampleRate, currentBlockSize);
    }

    const juce::ScopedLock sl (pluginLock);
    plugin = std::move (newPlugin);
}

juce::String Track::getPluginName() const
{
    if (plugin != nullptr)
        return plugin->getName();

    return {};
}

void Track::startRecording (double lengthBeats)
{
    // Piste vide => nouvelle boucle ; piste avec contenu => overdub (on garde tout).
    if (clip.isEmpty())
    {
        clip.clear();
        clip.setLengthBeats (lengthBeats);
    }

    std::memset (heldNotes, 0, sizeof (heldNotes));
    passStarts.clear();
    passStarts.push_back (clip.size()); // début de la première passe de cette prise
    loopState = LoopState::Recording;
    allNotesOffPending = true;
}

void Track::undoLastPass()
{
    if (! passStarts.empty())
    {
        const std::size_t target = passStarts.back();
        if (clip.size() > target)
        {
            clip.truncate (target);            // enlève la passe en cours
        }
        else if (passStarts.size() > 1)
        {
            passStarts.pop_back();
            clip.truncate (passStarts.back()); // enlève la passe précédente
        }
        else
        {
            clip.truncate (0);
        }
    }
    else
    {
        clip.clear();
    }

    std::memset (heldNotes, 0, sizeof (heldNotes));
    allNotesOffPending = true;

    if (clip.isEmpty() && loopState == LoopState::Playing)
        loopState = LoopState::Empty;
}

void Track::stopRecording()
{
    if (loopState == LoopState::Recording)
    {
        finishRecording();
        loopState = clip.isEmpty() ? LoopState::Empty : LoopState::Playing;
    }
}

void Track::clearLoop()
{
    clip.clear();
    loopState = LoopState::Empty;
    allNotesOffPending = true;
}

void Track::onTransportStopped()
{
    if (loopState == LoopState::Recording)
    {
        finishRecording();
        loopState = clip.isEmpty() ? LoopState::Empty : LoopState::Playing;
    }
    allNotesOffPending = true;
}

void Track::finishRecording()
{
    const double L       = clip.getLengthBeats();
    const double offBeat = (L > 1.0e-3 ? L - 1.0e-3 : 0.0);

    for (int ch = 0; ch < 16; ++ch)
        for (int note = 0; note < 128; ++note)
            if (heldNotes[ch][note])
            {
                const std::uint8_t off[3] = { (std::uint8_t) (0x80 | ch),
                                              (std::uint8_t) note, 0 };
                clip.addEvent (offBeat, off, 3);
                heldNotes[ch][note] = false;
            }

    allNotesOffPending = true;
}

void Track::renderBlock (const juce::MidiBuffer& liveMidi,
                         double linStart, double deltaBeats, double spb,
                         bool transportPlaying, int numSamples)
{
    // Ajuste la taille logique à numSamples (sans réallouer si la capacité suffit),
    // pour que processBlock du plugin traite exactement ce bloc.
    trackBuffer.setSize (2, numSamples, false, false, true);
    trackBuffer.clear();

    const double L      = clip.getLengthBeats();
    const double linEnd = linStart + deltaBeats;

    // Overdub : à chaque tour de boucle, on marque le début d'une nouvelle passe
    // (l'enregistrement ne s'arrête PAS tout seul ; c'est l'utilisateur qui stoppe).
    if (loopState == LoopState::Recording && transportPlaying && L > 0.0)
        if ((long) std::floor (linEnd / L) > (long) std::floor (linStart / L))
            passStarts.push_back (clip.size());

    // Enregistrement des événements live.
    if (loopState == LoopState::Recording && L > 0.0 && spb > 0.0)
    {
        const double loopLocalStart = std::fmod (linStart, L);
        for (const auto metadata : liveMidi)
        {
            const auto  msg = metadata.getMessage();
            const auto* raw = msg.getRawData();
            const int   n   = juce::jmin (3, msg.getRawDataSize());
            if (n <= 0 || raw[0] >= 0xF0)
                continue;

            const double evBeat = std::fmod (loopLocalStart
                                             + (double) metadata.samplePosition / spb, L);
            std::uint8_t bytes[3] = { 0, 0, 0 };
            for (int i = 0; i < n; ++i)
                bytes[i] = (std::uint8_t) raw[i];
            clip.addEvent (evBeat, bytes, n);

            const int ch = msg.getChannel();
            if (ch >= 1 && ch <= 16)
            {
                if (msg.isNoteOn())       heldNotes[ch - 1][msg.getNoteNumber()] = true;
                else if (msg.isNoteOff()) heldNotes[ch - 1][msg.getNoteNumber()] = false;
            }
        }
    }

    // MIDI combiné = live + lecture de la boucle.
    juce::MidiBuffer combined;
    combined.addEvents (liveMidi, 0, numSamples, 0);

    // On joue la boucle accumulée en lecture ET pendant l'overdub (pour entendre
    // les passes précédentes et empiler dessus).
    if ((loopState == LoopState::Playing || loopState == LoopState::Recording)
        && ! clip.isEmpty() && L > 0.0 && spb > 0.0)
    {
        const double loopLocalStart = std::fmod (linStart, L);
        const double to             = loopLocalStart + deltaBeats;

        auto emit = [&] (const med::ClipEvent& e, double offsetBeats)
        {
            if (e.numBytes <= 0)
                return;
            int off = (int) (offsetBeats * spb + 0.5);
            if (off < 0)           off = 0;
            if (off >= numSamples) off = numSamples - 1;
            combined.addEvent (juce::MidiMessage (e.bytes, e.numBytes), off);
        };

        if (to <= L)
        {
            clip.forEachInWindow (loopLocalStart, to, emit);
        }
        else
        {
            clip.forEachInWindow (loopLocalStart, L, emit);
            const double rem  = to - L;
            const double base = L - loopLocalStart;
            clip.forEachInWindow (0.0, rem, [&] (const med::ClipEvent& e, double off)
            {
                emit (e, base + off);
            });
        }
    }

    if (allNotesOffPending)
    {
        for (int ch = 1; ch <= 16; ++ch)
            combined.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
        allNotesOffPending = false;
    }

    // Rendu de l'instrument dans le buffer de la piste.
    {
        const juce::ScopedTryLock stl (pluginLock);
        if (stl.isLocked())
        {
            if (plugin != nullptr)
                plugin->processBlock (trackBuffer, combined);
            else
                synth.renderNextBlock (trackBuffer, combined, 0, numSamples);
        }
    }

    publishedState.store ((int) loopState);
}

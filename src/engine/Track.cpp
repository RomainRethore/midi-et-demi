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
    clip.reserve (med::LoopClip::maxNotes); // capacité fixe => pas de réallocation
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

    std::memset (noteHeld, 0, sizeof (noteHeld));
    loopState = LoopState::Recording;
    allNotesOffPending = true;
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

void Track::undo()
{
    if (clip.size() > 0)
        clip.truncate (clip.size() - 1);

    allNotesOffPending = true;
    if (clip.isEmpty() && loopState == LoopState::Playing)
        loopState = LoopState::Empty;
}

void Track::redo()
{
    clip.restore (clip.size() + 1);

    allNotesOffPending = true;
    if (! clip.isEmpty() && loopState == LoopState::Empty)
        loopState = LoopState::Playing;
}

void Track::finishRecording()
{
    // Ferme les notes encore tenues : elles vont jusqu'à la fin de la boucle.
    const double L = clip.getLengthBeats();
    for (int ch = 0; ch < 16; ++ch)
        for (int p = 0; p < 128; ++p)
            if (noteHeld[ch][p])
            {
                double len = L - noteStart[ch][p];
                if (len <= 0.0) len = L;
                clip.addNote ({ noteStart[ch][p], len,
                                (uint8_t) (ch + 1), (uint8_t) p, noteVel[ch][p] });
                noteHeld[ch][p] = false;
            }

    allNotesOffPending = true;
}

void Track::renderBlock (const juce::MidiBuffer& liveMidi,
                         double linStart, double deltaBeats, double spb,
                         bool transportPlaying, int numSamples)
{
    juce::ignoreUnused (transportPlaying);

    trackBuffer.setSize (2, numSamples, false, false, true);
    trackBuffer.clear();

    const double L = clip.getLengthBeats();

    // Enregistrement : on apparie note-on / note-off pour former des notes.
    if (loopState == LoopState::Recording && L > 0.0 && spb > 0.0)
    {
        const double loopLocalStart = std::fmod (linStart, L);
        for (const auto metadata : liveMidi)
        {
            const auto msg = metadata.getMessage();
            const double evBeat = std::fmod (loopLocalStart
                                             + (double) metadata.samplePosition / spb, L);
            const int ch = msg.getChannel();
            if (ch < 1 || ch > 16)
                continue;

            if (msg.isNoteOn())
            {
                const int p = msg.getNoteNumber();
                noteHeld[ch - 1][p]  = true;
                noteStart[ch - 1][p] = evBeat;
                noteVel[ch - 1][p]   = (uint8_t) msg.getVelocity();
            }
            else if (msg.isNoteOff())
            {
                const int p = msg.getNoteNumber();
                if (noteHeld[ch - 1][p])
                {
                    double len = evBeat - noteStart[ch - 1][p];
                    if (len <= 0.0)   len += L;             // note à cheval sur la boucle
                    if (len >= L)     len = L - 1.0e-3;     // borne (évite on==off)
                    clip.addNote ({ noteStart[ch - 1][p], len,
                                    (uint8_t) ch, (uint8_t) p, noteVel[ch - 1][p] });
                    noteHeld[ch - 1][p] = false;
                }
            }
        }
    }

    // MIDI combiné = live + lecture de la boucle (on entend aussi en overdub).
    juce::MidiBuffer combined;
    combined.addEvents (liveMidi, 0, numSamples, 0);

    if ((loopState == LoopState::Playing || loopState == LoopState::Recording)
        && ! clip.isEmpty() && L > 0.0 && spb > 0.0)
    {
        const double loopLocalStart = std::fmod (linStart, L);
        const double to             = loopLocalStart + deltaBeats;

        auto toOffset = [&] (double offsetBeats)
        {
            int off = (int) (offsetBeats * spb + 0.5);
            return juce::jlimit (0, numSamples - 1, off);
        };
        auto onFn  = [&] (const med::Note& n, double offBeats)
        {
            combined.addEvent (juce::MidiMessage::noteOn (n.channel, n.pitch,
                                                          (juce::uint8) n.velocity),
                               toOffset (offBeats));
        };
        auto offFn = [&] (const med::Note& n, double offBeats)
        {
            combined.addEvent (juce::MidiMessage::noteOff (n.channel, n.pitch),
                               toOffset (offBeats));
        };

        if (to <= L)
        {
            clip.emitWindow (loopLocalStart, to, onFn, offFn);
        }
        else
        {
            clip.emitWindow (loopLocalStart, L, onFn, offFn);
            const double rem  = to - L;
            const double base = L - loopLocalStart;
            clip.emitWindow (0.0, rem,
                             [&] (const med::Note& n, double o) { onFn (n, base + o); },
                             [&] (const med::Note& n, double o) { offFn (n, base + o); });
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

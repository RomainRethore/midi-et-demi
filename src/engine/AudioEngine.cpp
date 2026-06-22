#include "engine/AudioEngine.h"

#include <cmath>
#include <cstdint>
#include <cstring>

//==============================================================================
// EngineAudioSource
//==============================================================================
EngineAudioSource::EngineAudioSource (juce::MidiKeyboardState& keyStateToUse)
    : keyboardState (keyStateToUse)
{
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new SineVoice());

    synth.addSound (new SineSound());
}

void EngineAudioSource::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlockExpected;

    synth.setCurrentPlaybackSampleRate (sampleRate);
    midiCollector.reset (sampleRate);

    transport.prepare (sampleRate);
    transport.setTimeSignature (numerator, 4);
    metronome.prepare (sampleRate);

    clip.reserve (20000); // évite les réallocations pendant l'enregistrement

    const juce::ScopedLock sl (pluginLock);
    if (plugin != nullptr)
    {
        plugin->setPlayConfigDetails (0, 2, sampleRate, samplesPerBlockExpected);
        plugin->prepareToPlay (sampleRate, samplesPerBlockExpected);
    }
}

void EngineAudioSource::setPlugin (std::unique_ptr<juce::AudioPluginInstance> newPlugin)
{
    if (newPlugin != nullptr)
    {
        newPlugin->setPlayConfigDetails (0, 2, currentSampleRate, currentBlockSize);
        newPlugin->prepareToPlay (currentSampleRate, currentBlockSize);
    }

    const juce::ScopedLock sl (pluginLock);
    plugin = std::move (newPlugin);
}

void EngineAudioSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    const int numSamples  = bufferToFill.numSamples;
    const int startSample = bufferToFill.startSample;
    bufferToFill.clearActiveBufferRegion();

    // 1) MIDI live (clavier physique + écran), positions 0-based dans le bloc.
    juce::MidiBuffer liveMidi;
    midiCollector.removeNextBlockOfMessages (liveMidi, numSamples);
    keyboardState.processNextMidiBuffer (liveMidi, 0, numSamples, true);

    // 2) Commandes UI (capturées en début de bloc).
    const bool clearCmd  = clearPressed.exchange (false);
    const bool recordCmd = recordPressed.exchange (false);

    // 3) Transport : tempo + fronts lecture/arrêt.
    transport.setTempo (requestedBpm.load());
    const bool wantPlay = requestedPlaying.load();
    if (wantPlay && ! prevPlaying)
    {
        transport.rewind();
        transport.start();
        beatCounter = 0;
        nextBeatPos = 0.0;
        metronome.reset();
    }
    else if (! wantPlay && prevPlaying)
    {
        if (loopState == LoopState::Recording)
        {
            finishRecording();
            loopState = LoopState::Playing;
        }
        transport.stop();
        transport.rewind();
        metronome.reset();
        allNotesOffPending = true;
    }
    prevPlaying = wantPlay;

    // 4) Effacer / Enregistrer.
    if (clearCmd)
    {
        clip.clear();
        loopState = LoopState::Empty;
        allNotesOffPending = true;
    }
    if (recordCmd)
    {
        if (loopState == LoopState::Recording)
        {
            finishRecording();
            loopState = LoopState::Playing; // arrêt manuel de la prise
        }
        else
        {
            // Démarrage immédiat d'une prise, depuis le début.
            clip.clear();
            clip.setLengthBeats ((double) (requestedBars.load() * numerator));
            std::memset (heldNotes, 0, sizeof (heldNotes));
            transport.rewind();
            transport.start();
            requestedPlaying.store (true);
            prevPlaying = true;
            beatCounter = 0;
            nextBeatPos = 0.0;
            metronome.reset();
            allNotesOffPending = true;
            loopState = LoopState::Recording;
        }
    }

    // 5) Géométrie temporelle du bloc.
    const double spb        = transport.samplesPerBeat();
    const double deltaBeats = spb > 0.0 ? (double) numSamples / spb : 0.0;
    const double L          = clip.getLengthBeats();
    const double linStart   = transport.getPositionInBeats();
    const double linEnd      = linStart + deltaBeats;

    // Fin automatique de la prise après une boucle complète.
    if (transport.isPlaying() && L > 0.0 && loopState == LoopState::Recording)
    {
        const long idxStart = (long) std::floor (linStart / L);
        const long idxEnd   = (long) std::floor (linEnd   / L);
        if (idxEnd > idxStart)
        {
            finishRecording();
            loopState = LoopState::Playing;
        }
    }

    // 5) Enregistrement des événements live (si Recording).
    if (loopState == LoopState::Recording && L > 0.0 && spb > 0.0)
    {
        const double loopLocalStart = std::fmod (linStart, L);
        for (const auto metadata : liveMidi)
        {
            const auto msg   = metadata.getMessage();
            const auto* raw  = msg.getRawData();
            const int   n    = juce::jmin (3, msg.getRawDataSize());
            if (n <= 0 || raw[0] >= 0xF0) // on ignore les messages système/temps réel
                continue;

            const double evBeat = std::fmod (loopLocalStart
                                             + (double) metadata.samplePosition / spb, L);

            std::uint8_t bytes[3] = { 0, 0, 0 };
            for (int i = 0; i < n; ++i)
                bytes[i] = (std::uint8_t) raw[i];

            clip.addEvent (evBeat, bytes, n);

            // Suivi des notes tenues (pour fermer celles laissées ouvertes).
            const int ch = msg.getChannel();
            if (ch >= 1 && ch <= 16)
            {
                if (msg.isNoteOn())       heldNotes[ch - 1][msg.getNoteNumber()] = true;
                else if (msg.isNoteOff()) heldNotes[ch - 1][msg.getNoteNumber()] = false;
            }
        }
    }

    // 6) MIDI combiné = live + lecture de la boucle.
    juce::MidiBuffer combined;
    combined.addEvents (liveMidi, 0, numSamples, 0);

    if (loopState == LoopState::Playing && ! clip.isEmpty() && L > 0.0 && spb > 0.0)
    {
        const double loopLocalStart = std::fmod (linStart, L);
        const double to             = loopLocalStart + deltaBeats;

        auto emit = [&] (const med::ClipEvent& e, double offsetBeats)
        {
            if (e.numBytes <= 0)
                return;
            int off = (int) (offsetBeats * spb + 0.5);
            if (off < 0)            off = 0;
            if (off >= numSamples)  off = numSamples - 1;
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

    // 7) Instrument (plugin ou synthé) avec le MIDI combiné.
    {
        const juce::ScopedTryLock stl (pluginLock);
        if (stl.isLocked())
        {
            if (plugin != nullptr)
            {
                juce::AudioBuffer<float> proxy (bufferToFill.buffer->getArrayOfWritePointers(),
                                                bufferToFill.buffer->getNumChannels(),
                                                startSample, numSamples);
                plugin->processBlock (proxy, combined);
            }
            else
            {
                synth.renderNextBlock (*bufferToFill.buffer, combined, startSample, numSamples);
            }
        }
    }

    // 8) Métronome (per-sample, grille incrémentale).
    const bool   playing  = transport.isPlaying();
    const bool   metroOn  = metronomeEnabled.load();
    const double startPos = transport.getPositionSamples();
    const int    numCh    = bufferToFill.buffer->getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        if (playing)
        {
            const double absPos = startPos + (double) i;
            while (absPos >= nextBeatPos)
            {
                const bool downbeat = (beatCounter % numerator == 0);
                if (metroOn)
                    metronome.trigger (downbeat);

                ++beatCounter;
                nextBeatPos += spb;
            }
        }

        const float click = metroOn ? metronome.nextSample() : 0.0f;
        if (click != 0.0f)
            for (int ch = 0; ch < numCh; ++ch)
                bufferToFill.buffer->addSample (ch, startSample + i, click);
    }

    // 9) Avance + publication vers l'UI.
    transport.advance (numSamples);
    publishedBeats.store (transport.getPositionInBeats());
    publishedPlaying.store (transport.isPlaying());
    publishedLoopState.store ((int) loopState);
}

void EngineAudioSource::finishRecording()
{
    // Ferme proprement les notes restées enfoncées en fin de prise :
    // on inscrit leur note-off juste avant la fin de la boucle, et on coupe
    // le son courant de l'instrument.
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

//==============================================================================
// AudioEngine
//==============================================================================
AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    for (auto& input : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback (input.identifier, this);

    deviceManager.removeAudioCallback (&sourcePlayer);
    sourcePlayer.setSource (nullptr);
}

void AudioEngine::start()
{
    auto error = deviceManager.initialise (0, 2, nullptr, true);
    jassert (error.isEmpty());
    juce::ignoreUnused (error);

    sourcePlayer.setSource (&source);
    deviceManager.addAudioCallback (&sourcePlayer);

    openedMidiInputs.clear();
    for (auto& input : juce::MidiInput::getAvailableDevices())
    {
        if (! deviceManager.isMidiInputDeviceEnabled (input.identifier))
            deviceManager.setMidiInputDeviceEnabled (input.identifier, true);

        deviceManager.addMidiInputDeviceCallback (input.identifier, this);
        openedMidiInputs.add (input.name);
    }
}

juce::String AudioEngine::loadPluginFromFile (const juce::File& file)
{
    double sampleRate = 44100.0;
    int    blockSize  = 512;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        sampleRate = device->getCurrentSampleRate();
        blockSize  = device->getCurrentBufferSizeSamples();
    }

    juce::String errorMessage;
    auto instance = pluginHost.createFromFile (file, sampleRate, blockSize, errorMessage);

    if (instance == nullptr)
        return errorMessage.isNotEmpty() ? errorMessage
                                         : juce::String ("Echec du chargement du plugin.");

    source.setPlugin (std::move (instance));
    return {};
}

juce::String AudioEngine::getLoadedPluginName() const
{
    if (auto* p = source.getPlugin())
        return p->getName();

    return {};
}

juce::String AudioEngine::getStatusText()
{
    juce::String s;

    if (auto* device = deviceManager.getCurrentAudioDevice())
        s << "Audio      : " << device->getName()
          << "  (" << (int) device->getCurrentSampleRate() << " Hz, buffer "
          << device->getCurrentBufferSizeSamples() << " samples)\n";
    else
        s << "Audio      : aucun peripherique\n";

    if (openedMidiInputs.isEmpty())
        s << "MIDI       : aucun clavier detecte\n";
    else
        s << "MIDI       : " << openedMidiInputs.joinIntoString (", ") << "\n";

    auto pluginName = getLoadedPluginName();
    s << "Instrument : " << (pluginName.isNotEmpty()
                                ? pluginName
                                : juce::String ("synthe sinus (aucun plugin charge)"));
    return s;
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput* /*midiSource*/,
                                             const juce::MidiMessage& message)
{
    source.getMidiCollector()->addMessageToQueue (message);
}

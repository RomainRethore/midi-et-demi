#include "engine/AudioEngine.h"

#include <cmath>
#include <cstdint>

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

    // 2) Transport : tempo + fronts lecture/arrêt.
    transport.setTempo (requestedBpm.load());
    const bool wantPlay = requestedPlaying.load();
    if (wantPlay && ! prevPlaying)
    {
        transport.rewind();
        transport.start();
        beatCounter = 0;
        nextBeatPos = 0.0;
        metronome.reset();
        if (loopState == LoopState::Armed)
            loopState = LoopState::Recording; // enregistre dès le premier temps
    }
    else if (! wantPlay && prevPlaying)
    {
        transport.stop();
        transport.rewind();
        metronome.reset();
        if (loopState == LoopState::Recording)
            loopState = LoopState::Playing;
        allNotesOffPending = true;
    }
    prevPlaying = wantPlay;

    // 3) Commandes boucle (depuis l'UI).
    if (clearPressed.exchange (false))
    {
        clip.clear();
        loopState = LoopState::Empty;
        allNotesOffPending = true;
    }
    if (recordPressed.exchange (false))
    {
        switch (loopState)
        {
            case LoopState::Recording: loopState = LoopState::Playing; break;
            case LoopState::Armed:     loopState = clip.isEmpty() ? LoopState::Empty
                                                                  : LoopState::Playing; break;
            default: // Empty / Playing -> on (ré)arme une nouvelle prise
                clip.clear();
                clip.setLengthBeats ((double) (requestedBars.load() * numerator));
                loopState = LoopState::Armed;
                if (! requestedPlaying.load())
                    { /* en attente : démarrera au lancement de la lecture */ }
                break;
        }
    }

    // 4) Géométrie temporelle du bloc.
    const double spb        = transport.samplesPerBeat();
    const double deltaBeats = spb > 0.0 ? (double) numSamples / spb : 0.0;
    const double L          = clip.getLengthBeats();
    const double linStart   = transport.getPositionInBeats();
    const double linEnd      = linStart + deltaBeats;

    // Détection de frontière de boucle (changement de "loop index").
    if (transport.isPlaying() && L > 0.0)
    {
        const long idxStart = (long) std::floor (linStart / L);
        const long idxEnd   = (long) std::floor (linEnd   / L);
        if (idxEnd > idxStart)
        {
            if (loopState == LoopState::Armed)          loopState = LoopState::Recording;
            else if (loopState == LoopState::Recording) loopState = LoopState::Playing;
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
    publishedLoopState.store ((int) loopState);
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

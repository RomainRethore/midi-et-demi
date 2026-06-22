#include "engine/AudioEngine.h"

#include <cmath>

//==============================================================================
// EngineAudioSource
//==============================================================================
EngineAudioSource::EngineAudioSource (juce::MidiKeyboardState& keyStateToUse)
    : keyboardState (keyStateToUse)
{
    for (int i = 0; i < 8; ++i)        // 8 voix de secours (synthé sinus)
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
    bufferToFill.clearActiveBufferRegion();

    // 1) MIDI : clavier physique (file) + clavier à l'écran (état).
    juce::MidiBuffer incomingMidi;
    midiCollector.removeNextBlockOfMessages (incomingMidi, bufferToFill.numSamples);
    keyboardState.processNextMidiBuffer (incomingMidi, bufferToFill.startSample,
                                         bufferToFill.numSamples, true);

    // 2) Instrument : plugin si chargé, sinon synthé sinus.
    {
        const juce::ScopedTryLock stl (pluginLock);
        if (stl.isLocked())
        {
            if (plugin != nullptr)
            {
                juce::AudioBuffer<float> proxy (bufferToFill.buffer->getArrayOfWritePointers(),
                                                bufferToFill.buffer->getNumChannels(),
                                                bufferToFill.startSample,
                                                bufferToFill.numSamples);
                plugin->processBlock (proxy, incomingMidi);
            }
            else
            {
                synth.renderNextBlock (*bufferToFill.buffer, incomingMidi,
                                       bufferToFill.startSample, bufferToFill.numSamples);
            }
        }
    }

    // 3) Transport : on synchronise depuis l'UI, on gère les fronts lecture/arrêt.
    transport.setTempo (requestedBpm.load());

    const bool wantPlay = requestedPlaying.load();
    if (wantPlay && ! prevPlaying)
    {
        transport.rewind();
        transport.start();
        nextBeatIndex = 0;
        nextBeatPos   = 0.0;
        metronome.reset();
    }
    else if (! wantPlay && prevPlaying)
    {
        transport.stop();
        transport.rewind();
        metronome.reset();
    }
    prevPlaying = wantPlay;

    // 4) Métronome : on place les clics à l'échantillon près, et on les mixe.
    const bool   playing = transport.isPlaying();
    const bool   metroOn = metronomeEnabled.load();
    const double startPos = transport.getPositionSamples();
    const double spb       = transport.samplesPerBeat();
    const int    numCh     = bufferToFill.buffer->getNumChannels();

    for (int i = 0; i < bufferToFill.numSamples; ++i)
    {
        if (playing)
        {
            const double absPos = startPos + (double) i;
            while (absPos >= nextBeatPos)
            {
                const bool downbeat = (nextBeatIndex % numerator == 0);
                if (metroOn)
                    metronome.trigger (downbeat);

                ++nextBeatIndex;
                nextBeatPos = (double) nextBeatIndex * spb;
            }
        }

        const float click = metroOn ? metronome.nextSample() : 0.0f;
        if (click != 0.0f)
            for (int ch = 0; ch < numCh; ++ch)
                bufferToFill.buffer->addSample (ch, bufferToFill.startSample + i, click);
    }

    transport.advance (bufferToFill.numSamples);
    publishedBeats.store (transport.getPositionInBeats());
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
    deviceManager.addAudioCallback (&sourcePlayer); // déclenche prepareToPlay()

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

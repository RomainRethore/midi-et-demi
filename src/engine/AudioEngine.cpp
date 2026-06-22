#include "engine/AudioEngine.h"

//==============================================================================
// InstrumentSource
//==============================================================================
InstrumentSource::InstrumentSource (juce::MidiKeyboardState& keyStateToUse)
    : keyboardState (keyStateToUse)
{
    for (int i = 0; i < 8; ++i)        // 8 voix de secours (synthé sinus)
        synth.addVoice (new SineVoice());

    synth.addSound (new SineSound());
}

void InstrumentSource::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlockExpected;

    synth.setCurrentPlaybackSampleRate (sampleRate);
    midiCollector.reset (sampleRate);

    const juce::ScopedLock sl (pluginLock);
    if (plugin != nullptr)
    {
        plugin->setPlayConfigDetails (0, 2, sampleRate, samplesPerBlockExpected);
        plugin->prepareToPlay (sampleRate, samplesPerBlockExpected);
    }
}

void InstrumentSource::setPlugin (std::unique_ptr<juce::AudioPluginInstance> newPlugin)
{
    // Préparation (lourde, alloue) faite HORS du verrou, sur un plugin pas encore visible.
    if (newPlugin != nullptr)
    {
        newPlugin->setPlayConfigDetails (0, 2, currentSampleRate, currentBlockSize);
        newPlugin->prepareToPlay (currentSampleRate, currentBlockSize);
    }

    // Échange court sous verrou ; l'ancien plugin est détruit ici, le fil audio
    // étant tenu à l'écart par son try-lock pendant ce temps.
    const juce::ScopedLock sl (pluginLock);
    plugin = std::move (newPlugin);
}

void InstrumentSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    // MIDI : clavier physique (file) + clavier à l'écran (état). Hors verrou.
    juce::MidiBuffer incomingMidi;
    midiCollector.removeNextBlockOfMessages (incomingMidi, bufferToFill.numSamples);
    keyboardState.processNextMidiBuffer (incomingMidi, bufferToFill.startSample,
                                         bufferToFill.numSamples, true);

    const juce::ScopedTryLock stl (pluginLock);
    if (stl.isLocked())
    {
        if (plugin != nullptr)
        {
            // Vue sur la sous-région du buffer de sortie pour le plugin.
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
    // Verrou indisponible (changement d'instrument en cours) => silence sur ce bloc.
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

    sourcePlayer.setSource (&instrument);
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

    juce::String error;
    auto instance = pluginHost.createFromFile (file, sampleRate, blockSize, error);

    if (instance == nullptr)
        return error.isNotEmpty() ? error : juce::String ("Echec du chargement du plugin.");

    instrument.setPlugin (std::move (instance));
    return {};
}

juce::String AudioEngine::getLoadedPluginName() const
{
    if (auto* p = instrument.getPlugin())
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

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput* /*source*/,
                                             const juce::MidiMessage& message)
{
    instrument.getMidiCollector()->addMessageToQueue (message);
}

#include "engine/AudioEngine.h"

//==============================================================================
// SynthAudioSource
//==============================================================================
SynthAudioSource::SynthAudioSource (juce::MidiKeyboardState& keyStateToUse)
    : keyboardState (keyStateToUse)
{
    for (int i = 0; i < 8; ++i)        // 8 voix => polyphonie de 8 notes
        synth.addVoice (new SineVoice());

    synth.addSound (new SineSound());
}

void SynthAudioSource::prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    midiCollector.reset (sampleRate);
}

void SynthAudioSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    juce::MidiBuffer incomingMidi;

    // 1) le MIDI du clavier physique (déposé par handleIncomingMidiMessage)
    midiCollector.removeNextBlockOfMessages (incomingMidi, bufferToFill.numSamples);

    // 2) le MIDI du clavier à l'écran (true => injecte aussi les événements indirects)
    keyboardState.processNextMidiBuffer (incomingMidi, bufferToFill.startSample,
                                         bufferToFill.numSamples, true);

    // 3) on rend les notes
    synth.renderNextBlock (*bufferToFill.buffer, incomingMidi,
                           bufferToFill.startSample, bufferToFill.numSamples);
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
    // 0 entrée, 2 sorties (stéréo). Le "true" sélectionne le périphérique par défaut.
    auto error = deviceManager.initialise (0, 2, nullptr, true);
    jassert (error.isEmpty());
    juce::ignoreUnused (error);

    sourcePlayer.setSource (&synthSource);
    deviceManager.addAudioCallback (&sourcePlayer); // déclenche prepareToPlay()

    // On connecte TOUS les claviers MIDI présents (dont l'Oxygen Pro Mini).
    openedMidiInputs.clear();
    for (auto& input : juce::MidiInput::getAvailableDevices())
    {
        if (! deviceManager.isMidiInputDeviceEnabled (input.identifier))
            deviceManager.setMidiInputDeviceEnabled (input.identifier, true);

        deviceManager.addMidiInputDeviceCallback (input.identifier, this);
        openedMidiInputs.add (input.name);
    }
}

juce::String AudioEngine::getStatusText()
{
    juce::String s;

    if (auto* device = deviceManager.getCurrentAudioDevice())
        s << "Audio : " << device->getName()
          << "  (" << (int) device->getCurrentSampleRate() << " Hz, buffer "
          << device->getCurrentBufferSizeSamples() << " samples)\n";
    else
        s << "Audio : aucun peripherique\n";

    if (openedMidiInputs.isEmpty())
        s << "MIDI  : aucun clavier detecte";
    else
        s << "MIDI  : " << openedMidiInputs.joinIntoString (", ");

    return s;
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput* /*source*/,
                                             const juce::MidiMessage& message)
{
    // Appelé sur le fil MIDI : on dépose juste le message dans la file lock-free.
    synthSource.getMidiCollector()->addMessageToQueue (message);
}

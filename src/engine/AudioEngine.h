#pragma once

#include <JuceHeader.h>
#include "engine/SineSynth.h"

/**
    Source audio qui rend le synthé à partir du MIDI entrant (clavier physique
    + clavier à l'écran). C'est elle qui est appelée par le fil audio temps réel.
*/
class SynthAudioSource : public juce::AudioSource
{
public:
    explicit SynthAudioSource (juce::MidiKeyboardState& keyStateToUse);

    /** File thread-safe par laquelle on injecte le MIDI du clavier physique. */
    juce::MidiMessageCollector* getMidiCollector() noexcept { return &midiCollector; }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override {}
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    juce::MidiKeyboardState&  keyboardState;
    juce::Synthesiser         synth;
    juce::MidiMessageCollector midiCollector;
};

/**
    Couche MOTEUR (engine). Possède la carte son, le synthé et l'entrée MIDI.
    À ce stade (1a), elle ne dépend de rien au-dessus d'elle : ni UI, ni
    application. Elle sera amenée à exposer des interfaces plus tard.
*/
class AudioEngine : public juce::MidiInputCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    /** Ouvre la sortie audio et connecte tous les claviers MIDI disponibles. */
    void start();

    /** État partagé avec le clavier à l'écran (UI). */
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    /** Texte de statut (périphérique audio, claviers MIDI détectés). */
    juce::String getStatusText();

    // --- juce::MidiInputCallback (appelé sur le fil MIDI) ---
    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

private:
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer  sourcePlayer;
    juce::MidiKeyboardState  keyboardState;
    SynthAudioSource         synthSource { keyboardState };
    juce::StringArray        openedMidiInputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

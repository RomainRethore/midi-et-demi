#pragma once

#include <JuceHeader.h>
#include "engine/SineSynth.h"
#include "engine/PluginHost.h"

/**
    Source audio appelée par le fil temps réel. Joue le plugin instrument s'il
    est chargé, sinon le synthé sinus de secours. Le plugin est protégé par un
    verrou : on le pose côté fil message, et le fil audio n'utilise qu'un
    try-lock (jamais d'attente bloquante => pas de coupure de son).
*/
class InstrumentSource : public juce::AudioSource
{
public:
    explicit InstrumentSource (juce::MidiKeyboardState& keyStateToUse);

    /** File thread-safe par laquelle on injecte le MIDI du clavier physique. */
    juce::MidiMessageCollector* getMidiCollector() noexcept { return &midiCollector; }

    /** Remplace l'instrument courant (appelé depuis le fil message). */
    void setPlugin (std::unique_ptr<juce::AudioPluginInstance> newPlugin);

    /** Pointeur sur le plugin courant (pour créer son éditeur). Peut être nul. */
    juce::AudioPluginInstance* getPlugin() const noexcept { return plugin.get(); }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override {}
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    juce::MidiKeyboardState&   keyboardState;
    juce::Synthesiser          synth;
    juce::MidiMessageCollector midiCollector;

    juce::CriticalSection      pluginLock;
    std::unique_ptr<juce::AudioPluginInstance> plugin;

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentSource)
};

/**
    Couche MOTEUR (engine). Possède la carte son, l'instrument (plugin ou synthé)
    et l'entrée MIDI. Sa façade publique reste simple : start / load / état.
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

    /** Charge un plugin depuis un fichier. Retourne "" si OK, sinon le message d'erreur. */
    juce::String loadPluginFromFile (const juce::File& file);

    /** Plugin courant (pour ouvrir son éditeur). Peut être nul. */
    juce::AudioPluginInstance* getLoadedPlugin() const noexcept { return instrument.getPlugin(); }

    juce::String getLoadedPluginName() const;
    juce::String getStatusText();

    // --- juce::MidiInputCallback (appelé sur le fil MIDI) ---
    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

private:
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer  sourcePlayer;
    juce::MidiKeyboardState  keyboardState;
    InstrumentSource         instrument { keyboardState };
    PluginHost               pluginHost;
    juce::StringArray        openedMidiInputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "domain/Transport.h"
#include "engine/SineSynth.h"
#include "engine/Metronome.h"
#include "engine/PluginHost.h"

/**
    Source audio appelée par le fil temps réel. Elle :
      - rend l'instrument (plugin chargé, sinon synthé sinus de secours),
      - fait avancer l'horloge musicale (Transport),
      - place les clics du métronome à l'échantillon près.

    Les contrôles venant de l'UI (BPM, lecture, métronome) passent par des
    variables atomiques, lues en début de bloc : pas de verrou sur le chemin
    audio (hormis l'échange de plugin, protégé par try-lock).
*/
class EngineAudioSource : public juce::AudioSource
{
public:
    explicit EngineAudioSource (juce::MidiKeyboardState& keyStateToUse);

    juce::MidiMessageCollector* getMidiCollector() noexcept { return &midiCollector; }

    void setPlugin (std::unique_ptr<juce::AudioPluginInstance> newPlugin);
    juce::AudioPluginInstance* getPlugin() const noexcept { return plugin.get(); }

    // --- contrôles transport (appelés depuis le fil message) ---
    void setTempo (double bpm) noexcept            { requestedBpm.store (bpm); }
    void setPlaying (bool shouldPlay) noexcept      { requestedPlaying.store (shouldPlay); }
    void setMetronomeEnabled (bool on) noexcept     { metronomeEnabled.store (on); }
    double getPositionInBeats() const noexcept      { return publishedBeats.load(); }
    int    getNumerator() const noexcept            { return numerator; }

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

    // --- horloge + métronome (état détenu par le fil audio) ---
    med::Transport transport;
    med::Metronome metronome;
    static constexpr int numerator = 4; // signature 4/4 fixe pour l'étape 2
    bool   prevPlaying  = false;
    long   nextBeatIndex = 0;
    double nextBeatPos   = 0.0;

    // --- échanges avec l'UI ---
    std::atomic<double> requestedBpm     { 120.0 };
    std::atomic<bool>   requestedPlaying { false };
    std::atomic<bool>   metronomeEnabled { true };
    std::atomic<double> publishedBeats   { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineAudioSource)
};

/**
    Couche MOTEUR (engine). Possède la carte son, l'instrument, l'entrée MIDI,
    et expose le transport. Façade simple pour l'UI.
*/
class AudioEngine : public juce::MidiInputCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void start();

    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    juce::String loadPluginFromFile (const juce::File& file);
    juce::AudioPluginInstance* getLoadedPlugin() const noexcept { return source.getPlugin(); }
    juce::String getLoadedPluginName() const;
    juce::String getStatusText();

    // --- transport ---
    void   setTempo (double bpm) noexcept        { source.setTempo (bpm); }
    void   setPlaying (bool shouldPlay) noexcept { source.setPlaying (shouldPlay); }
    void   setMetronomeEnabled (bool on) noexcept { source.setMetronomeEnabled (on); }
    double getPositionInBeats() const noexcept   { return source.getPositionInBeats(); }
    int    getNumerator() const noexcept         { return source.getNumerator(); }

    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

private:
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer  sourcePlayer;
    juce::MidiKeyboardState  keyboardState;
    EngineAudioSource        source { keyboardState };
    PluginHost               pluginHost;
    juce::StringArray        openedMidiInputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "domain/Transport.h"
#include "domain/LoopClip.h"
#include "engine/SineSynth.h"
#include "engine/Metronome.h"
#include "engine/PluginHost.h"

/**
    Source audio (fil temps réel). Elle :
      - rend l'instrument (plugin chargé, sinon synthé sinus),
      - fait avancer l'horloge (Transport) et place les clics du métronome,
      - enregistre une boucle MIDI puis la rejoue en boucle (étape 3).

    États de la boucle : Vide -> Armee -> Enregistrement -> Lecture.
    Les contrôles UI passent par des atomiques ; la boucle (LoopClip) n'est
    touchée que par le fil audio.
*/
class EngineAudioSource : public juce::AudioSource
{
public:
    enum class LoopState { Empty = 0, Armed = 1, Recording = 2, Playing = 3 };

    explicit EngineAudioSource (juce::MidiKeyboardState& keyStateToUse);

    juce::MidiMessageCollector* getMidiCollector() noexcept { return &midiCollector; }

    void setPlugin (std::unique_ptr<juce::AudioPluginInstance> newPlugin);
    juce::AudioPluginInstance* getPlugin() const noexcept { return plugin.get(); }

    // --- transport ---
    void setTempo (double bpm) noexcept        { requestedBpm.store (bpm); }
    void setPlaying (bool shouldPlay) noexcept  { requestedPlaying.store (shouldPlay); }
    void setMetronomeEnabled (bool on) noexcept { metronomeEnabled.store (on); }
    double getPositionInBeats() const noexcept  { return publishedBeats.load(); }
    bool   isPlaying() const noexcept           { return publishedPlaying.load(); }
    int    getNumerator() const noexcept        { return numerator; }

    // --- boucle ---
    void setLoopBars (int bars) noexcept { requestedBars.store (bars); }
    void pressRecord() noexcept          { recordPressed.store (true); }
    void pressClear() noexcept           { clearPressed.store (true); }
    int  getLoopState() const noexcept   { return publishedLoopState.load(); }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override {}
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    /** Termine une prise : écrit les note-off des notes encore tenues. */
    void finishRecording();

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
    static constexpr int numerator = 4; // signature 4/4 fixe pour l'instant
    bool   prevPlaying = false;
    long   beatCounter = 0;
    double nextBeatPos = 0.0;

    // --- boucle (état détenu par le fil audio) ---
    med::LoopClip clip;
    LoopState     loopState = LoopState::Empty;
    bool          allNotesOffPending = false;
    bool          heldNotes[16][128] = {}; // notes tenues pendant l'enregistrement

    // --- échanges avec l'UI ---
    std::atomic<double> requestedBpm      { 120.0 };
    std::atomic<bool>   requestedPlaying  { false };
    std::atomic<bool>   metronomeEnabled  { true };
    std::atomic<double> publishedBeats     { 0.0 };
    std::atomic<bool>   publishedPlaying   { false };

    std::atomic<int>    requestedBars      { 4 };
    std::atomic<bool>   recordPressed      { false };
    std::atomic<bool>   clearPressed       { false };
    std::atomic<int>    publishedLoopState { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineAudioSource)
};

/**
    Couche MOTEUR (engine). Façade simple pour l'UI : carte son, instrument,
    entrée MIDI, transport et boucle.
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
    void   setTempo (double bpm) noexcept         { source.setTempo (bpm); }
    void   setPlaying (bool shouldPlay) noexcept  { source.setPlaying (shouldPlay); }
    void   setMetronomeEnabled (bool on) noexcept { source.setMetronomeEnabled (on); }
    double getPositionInBeats() const noexcept    { return source.getPositionInBeats(); }
    bool   isPlaying() const noexcept             { return source.isPlaying(); }
    int    getNumerator() const noexcept          { return source.getNumerator(); }

    // --- boucle ---
    void setLoopBars (int bars) noexcept { source.setLoopBars (bars); }
    void pressRecord() noexcept          { source.pressRecord(); }
    void pressClear() noexcept           { source.pressClear(); }
    int  getLoopState() const noexcept   { return source.getLoopState(); }

    void handleIncomingMidiMessage (juce::MidiInput* midiSource,
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

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>
#include "domain/Transport.h"
#include "engine/Metronome.h"
#include "engine/PluginHost.h"
#include "engine/Track.h"

/**
    Source audio (fil temps réel) : 8 pistes indépendantes, une horloge et un
    métronome partagés. Le MIDI live (clavier physique + écran) va à la piste
    ACTIVE ; chaque piste rend son instrument + sa boucle, puis on mixe le tout.
*/
class EngineAudioSource : public juce::AudioSource
{
public:
    static constexpr int numTracks = 8;

    explicit EngineAudioSource (juce::MidiKeyboardState& keyStateToUse);

    juce::MidiMessageCollector* getMidiCollector() noexcept { return &midiCollector; }

    Track& getTrack (int index) noexcept { return tracks[(size_t) index]; }

    // --- transport ---
    void setTempo (double bpm) noexcept         { requestedBpm.store (bpm); }
    void setPlaying (bool shouldPlay) noexcept  { requestedPlaying.store (shouldPlay); }
    void setMetronomeEnabled (bool on) noexcept { metronomeEnabled.store (on); }
    double getPositionInBeats() const noexcept  { return publishedBeats.load(); }
    bool   isPlaying() const noexcept           { return publishedPlaying.load(); }
    int    getNumerator() const noexcept        { return numerator; }

    // --- pistes / boucle ---
    void setActiveTrack (int index) noexcept { activeTrack.store (index); }
    int  getActiveTrack() const noexcept     { return activeTrack.load(); }
    void pressRecord() noexcept              { recordPressed.store (true); }
    void pressClear() noexcept               { clearPressed.store (true); }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override {}
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    juce::MidiKeyboardState&   keyboardState;
    juce::MidiMessageCollector midiCollector;

    std::array<Track, numTracks> tracks;

    med::Transport transport;
    med::Metronome metronome;
    static constexpr int numerator = 4; // signature 4/4 fixe pour l'instant
    bool   prevPlaying = false;
    long   beatCounter = 0;
    double nextBeatPos = 0.0;

    std::atomic<double> requestedBpm      { 120.0 };
    std::atomic<bool>   requestedPlaying  { false };
    std::atomic<bool>   metronomeEnabled  { true };
    std::atomic<double> publishedBeats     { 0.0 };
    std::atomic<bool>   publishedPlaying   { false };
    std::atomic<int>    activeTrack        { 0 };
    std::atomic<bool>   recordPressed      { false };
    std::atomic<bool>   clearPressed       { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineAudioSource)
};

/**
    Couche MOTEUR (engine). Façade simple pour l'UI : carte son, pistes,
    entrée MIDI, transport. Les opérations « plugin / enregistrer / effacer »
    s'appliquent à la piste ACTIVE.
*/
class AudioEngine : public juce::MidiInputCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void start();

    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    /** Charge un plugin dans la piste active. "" si OK, sinon message d'erreur. */
    juce::String loadPluginToActiveTrack (const juce::File& file);
    juce::AudioPluginInstance* getActivePlugin() noexcept;
    juce::String getStatusText();

    // --- transport ---
    void   setTempo (double bpm) noexcept         { source.setTempo (bpm); }
    void   setPlaying (bool shouldPlay) noexcept  { source.setPlaying (shouldPlay); }
    void   setMetronomeEnabled (bool on) noexcept { source.setMetronomeEnabled (on); }
    double getPositionInBeats() const noexcept    { return source.getPositionInBeats(); }
    bool   isPlaying() const noexcept             { return source.isPlaying(); }
    int    getNumerator() const noexcept          { return source.getNumerator(); }

    // --- pistes ---
    int  numTracks() const noexcept          { return EngineAudioSource::numTracks; }
    void setActiveTrack (int i) noexcept      { source.setActiveTrack (i); }
    int  getActiveTrack() const noexcept      { return source.getActiveTrack(); }
    void pressRecord() noexcept               { source.pressRecord(); }
    void pressClear() noexcept                { source.pressClear(); }

    void   setTrackVolume (int i, float v) noexcept { source.getTrack (i).setVolume (v); }
    float  getTrackVolume (int i) noexcept          { return source.getTrack (i).getVolume(); }
    void   setTrackMute (int i, bool m) noexcept    { source.getTrack (i).setMute (m); }
    bool   isTrackMuted (int i) noexcept            { return source.getTrack (i).isMuted(); }
    void   setTrackBars (int i, int b) noexcept     { source.getTrack (i).setBars (b); }
    int    getTrackBars (int i) noexcept            { return source.getTrack (i).getBars(); }
    int    getTrackLoopState (int i) noexcept       { return source.getTrack (i).getLoopState(); }
    juce::String getTrackPluginName (int i)         { return source.getTrack (i).getPluginName(); }

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

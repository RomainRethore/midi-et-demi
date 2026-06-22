#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "domain/LoopClip.h"
#include "engine/SineSynth.h"

/**
    Une PISTE (couche engine) : son propre instrument (plugin ou synthé sinus),
    sa boucle MIDI, son volume / mute, et sa machine d'état d'enregistrement.
    Rendue dans son buffer interne, puis mixée par le moteur.

    État détenu par le fil audio ; les contrôles UI passent par des atomiques.
*/
class Track
{
public:
    enum class LoopState { Empty = 0, Recording = 2, Playing = 3 };

    Track();

    void prepare (double sampleRate, int blockSize);

    void setPlugin (std::unique_ptr<juce::AudioPluginInstance> newPlugin);
    juce::AudioPluginInstance* getPlugin() const noexcept { return plugin.get(); }
    juce::String getPluginName() const;

    // --- commandes (fil audio, appelées par le moteur) ---
    void startRecording (double lengthBeats);
    void stopRecording();
    void clearLoop();
    void onTransportStopped();
    void undoLastPass();

    /** Coupe les notes en cours au prochain bloc (ex. avant un réalignement). */
    void requestAllNotesOff() noexcept { allNotesOffPending = true; }

    /** Rend un bloc dans le buffer interne (0-based, numSamples). */
    void renderBlock (const juce::MidiBuffer& liveMidi,
                      double linStart, double deltaBeats, double spb,
                      bool transportPlaying, int numSamples);

    const juce::AudioBuffer<float>& getOutput() const noexcept { return trackBuffer; }

    /** Copie les événements + la longueur pour la visualisation (appelé par l'UI). */
    void getDisplayData (std::vector<med::ClipEvent>& out, double& lengthBeats) const
    {
        lengthBeats = clip.getLengthBeats();
        const auto& e = clip.getEvents();
        out.assign (e.begin(), e.end());
    }

    // --- contrôles UI (atomiques) ---
    void  setVolume (float v) noexcept { volume.store (v); }
    float getVolume() const noexcept   { return volume.load(); }
    void  setMute (bool m) noexcept    { muted.store (m); }
    bool  isMuted() const noexcept     { return muted.load(); }
    void  setBars (int b) noexcept     { bars.store (b); }
    int   getBars() const noexcept     { return bars.load(); }
    int   getLoopState() const noexcept { return publishedState.load(); }

private:
    void finishRecording();

    juce::Synthesiser          synth;
    juce::CriticalSection      pluginLock;
    std::unique_ptr<juce::AudioPluginInstance> plugin;

    med::LoopClip clip;
    LoopState     loopState = LoopState::Empty;
    bool          heldNotes[16][128] = {};
    bool          allNotesOffPending = false;
    std::vector<std::size_t> passStarts; // taille du clip au début de chaque passe (undo)

    juce::AudioBuffer<float> trackBuffer;
    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    std::atomic<float> volume         { 0.8f };
    std::atomic<bool>  muted          { false };
    std::atomic<int>   bars           { 4 };
    std::atomic<int>   publishedState { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Track)
};

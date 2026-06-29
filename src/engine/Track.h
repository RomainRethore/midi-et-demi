#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "domain/LoopClip.h"
#include "engine/SineSynth.h"

/**
    Une PISTE (couche engine) : son instrument (plugin ou synthé sinus), sa
    boucle MIDI (stockée par note), son volume / mute, sa machine d'état
    d'enregistrement. Undo/redo se font NOTE PAR NOTE.
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

    // --- commandes (fil audio) ---
    void startRecording (double lengthBeats);
    void stopRecording();
    void clearLoop();
    void onTransportStopped();
    void undo(); // retire la dernière note
    void redo(); // remet la dernière note annulée

    void requestAllNotesOff() noexcept { allNotesOffPending = true; }

    void renderBlock (const juce::MidiBuffer& liveMidi,
                      double linStart, double deltaBeats, double spb,
                      bool transportPlaying, int numSamples);

    const juce::AudioBuffer<float>& getOutput() const noexcept { return trackBuffer; }

    /** Copie les notes + la longueur pour la visualisation (appelé par l'UI). */
    void getDisplayData (std::vector<med::Note>& out, double& lengthBeats) const
    {
        lengthBeats = clip.getLengthBeats();
        const auto& n = clip.getNotes();
        out.assign (n.begin(), n.begin() + (long) clip.getUsedCount());
    }

    /** Export complet pour la sauvegarde de session. */
    void getSaveData (double& lengthBeats,
                      std::vector<med::Note>& notes,
                      std::vector<med::CtrlEvent>& controls) const
    {
        lengthBeats = clip.getLengthBeats();
        const auto& n = clip.getNotes();
        notes.assign (n.begin(), n.begin() + (long) clip.getUsedCount());
        const auto& c = clip.getControls();
        controls.assign (c.begin(), c.end());
    }

    /** Import complet (au chargement de session). À appeler hors fil audio. */
    void loadClip (double lengthBeats,
                   const std::vector<med::Note>& notes,
                   const std::vector<med::CtrlEvent>& controls);

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
    bool          allNotesOffPending = false;

    // Notes en cours d'enregistrement (note-on en attente de note-off).
    bool    noteHeld  [16][128] = {};
    double  noteStart [16][128] = {};
    uint8_t noteVel   [16][128] = {};

    juce::AudioBuffer<float> trackBuffer;
    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    std::atomic<float> volume         { 0.8f };
    std::atomic<bool>  muted          { false };
    std::atomic<int>   bars           { 4 };
    std::atomic<int>   publishedState { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Track)
};

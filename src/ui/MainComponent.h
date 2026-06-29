#pragma once

#include <JuceHeader.h>
#include <array>
#include "engine/AudioEngine.h"
#include "ui/PluginEditorWindow.h"
#include "ui/LoopLanes.h"
#include "ui/MappingWindow.h"

/**
    Couche UI (étape 4a). Transport partagé + sélecteur de PISTE ACTIVE (1-8) :
    les contrôles « plugin / mesures / enregistrer / effacer / volume / mute »
    agissent sur la piste active. Ne connaît que la façade de AudioEngine.
*/
class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectTrack (int index);
    void refreshActiveControls();
    void openPluginFile();
    void showPluginEditor();

    AudioEngine engine;

    juce::MidiKeyboardComponent keyboard { engine.getKeyboardState(),
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    juce::Label titleLabel;
    juce::Label statusLabel;

    // transport
    juce::TextButton   playButton   { "Lecture" };
    juce::Slider       bpmSlider;
    juce::Label        bpmLabel      { {}, "Tempo" };
    juce::Label        bpmUnitLabel  { {}, "BPM" };
    juce::ToggleButton metronomeToggle { "Metronome" };
    juce::Label        positionLabel;

    // sélecteur de piste
    juce::Label                      tracksLabel { {}, "Piste active" };
    std::array<juce::TextButton, 8>  trackButtons;

    // contrôles de la piste active
    juce::TextButton   loadButton   { "Charger un plugin..." };
    juce::TextButton   editorButton { "Editeur" };
    juce::Label        barsLabel    { {}, "Mesures" };
    juce::ComboBox     barsCombo;
    juce::TextButton   recordButton { "Enregistrer" };
    juce::TextButton   undoButton   { "Annuler" };
    juce::TextButton   redoButton   { "Refaire" };
    juce::TextButton   clearButton  { "Effacer" };
    juce::Label        volumeLabel  { {}, "Volume" };
    juce::Slider       volumeSlider;
    juce::ToggleButton muteToggle   { "Mute" };
    juce::TextButton   mappingButton { "Mapping..." };
    juce::Label        activeInfoLabel;

    LoopLanes          loopLanes { engine };

    std::unique_ptr<juce::FileChooser>  fileChooser;
    std::unique_ptr<PluginEditorWindow> pluginWindow;
    std::unique_ptr<MappingWindow>      mappingWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

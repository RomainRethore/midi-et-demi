#pragma once

#include <JuceHeader.h>
#include "engine/AudioEngine.h"
#include "ui/PluginEditorWindow.h"

/**
    Couche UI. Transport (Lecture/Stop, BPM, métronome, position), chargement de
    plugin + éditeur, et clavier à l'écran. Ne connaît que la façade de AudioEngine.
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
    void togglePlay();
    void openPluginFile();
    void showPluginEditor();

    AudioEngine engine;

    juce::MidiKeyboardComponent keyboard { engine.getKeyboardState(),
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    juce::Label        titleLabel;
    juce::Label        statusLabel;

    // transport
    juce::TextButton   playButton   { "Lecture" };
    juce::Slider       bpmSlider;
    juce::Label        bpmLabel      { {}, "Tempo" };
    juce::Label        bpmUnitLabel  { {}, "BPM" };
    juce::ToggleButton metronomeToggle { "Metronome" };
    juce::Label        positionLabel;
    bool               isPlaying = false;

    // boucle
    juce::Label        barsLabel     { {}, "Mesures" };
    juce::ComboBox     barsCombo;
    juce::TextButton   recordButton  { "Enregistrer" };
    juce::TextButton   clearButton   { "Effacer" };
    juce::Label        loopStateLabel;

    // plugin
    juce::TextButton   loadButton   { "Charger un plugin..." };
    juce::TextButton   editorButton { "Ouvrir l'editeur" };

    std::unique_ptr<juce::FileChooser>  fileChooser;
    std::unique_ptr<PluginEditorWindow> pluginWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

#pragma once

#include <JuceHeader.h>
#include "engine/AudioEngine.h"
#include "ui/PluginEditorWindow.h"

/**
    Couche UI. Clavier à l'écran, statut, et boutons pour charger un plugin
    instrument et ouvrir son interface. Ne connaît que la façade de AudioEngine.
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
    void openPluginFile();
    void showPluginEditor();

    AudioEngine engine;

    juce::MidiKeyboardComponent keyboard { engine.getKeyboardState(),
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    juce::Label      titleLabel;
    juce::Label      statusLabel;
    juce::TextButton loadButton   { "Charger un plugin..." };
    juce::TextButton editorButton { "Ouvrir l'editeur" };

    std::unique_ptr<juce::FileChooser>  fileChooser;
    std::unique_ptr<PluginEditorWindow> pluginWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

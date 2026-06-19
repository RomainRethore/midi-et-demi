#pragma once

#include <JuceHeader.h>
#include "engine/AudioEngine.h"

/**
    Couche UI. Affiche un clavier à l'écran et un texte de statut.
    Elle ne connaît que la façade publique de AudioEngine (start / état /
    keyboardState) — pas ses détails internes.
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

    AudioEngine engine;

    juce::MidiKeyboardComponent keyboard { engine.getKeyboardState(),
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    juce::Label titleLabel;
    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

#include "ui/MainComponent.h"

MainComponent::MainComponent()
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("Midi et demi - squelette audio (etape 1a)",
                        juce::dontSendNotification);
    titleLabel.setFont (juce::Font (20.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (keyboard);

    engine.start();
    startTimerHz (2); // rafraîchit le statut 2x/seconde

    setSize (760, 320);
}

MainComponent::~MainComponent()
{
    stopTimer();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (12);
    titleLabel .setBounds (area.removeFromTop (32));
    statusLabel.setBounds (area.removeFromTop (52));
    keyboard   .setBounds (area.removeFromBottom (140));
}

void MainComponent::timerCallback()
{
    statusLabel.setText (engine.getStatusText(), juce::dontSendNotification);
}

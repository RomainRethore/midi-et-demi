#include "ui/MainComponent.h"

MainComponent::MainComponent()
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("Midi et demi - hote de plugin (etape 1b)",
                        juce::dontSendNotification);
    titleLabel.setFont (juce::Font (20.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::topLeft);

    addAndMakeVisible (loadButton);
    loadButton.onClick = [this] { openPluginFile(); };

    addAndMakeVisible (editorButton);
    editorButton.onClick = [this] { showPluginEditor(); };

    addAndMakeVisible (keyboard);

    engine.start();
    startTimerHz (2); // rafraîchit le statut 2x/seconde

    setSize (780, 400);
}

MainComponent::~MainComponent()
{
    stopTimer();
    pluginWindow = nullptr; // ferme l'éditeur avant que l'engine (et le plugin) ne disparaisse
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (12);

    titleLabel.setBounds (area.removeFromTop (32));
    statusLabel.setBounds (area.removeFromTop (72));

    auto buttons = area.removeFromTop (40);
    loadButton  .setBounds (buttons.removeFromLeft (200).reduced (2));
    editorButton.setBounds (buttons.removeFromLeft (200).reduced (2));

    keyboard.setBounds (area.removeFromBottom (140));
}

void MainComponent::timerCallback()
{
    statusLabel.setText (engine.getStatusText(), juce::dontSendNotification);
}

void MainComponent::openPluginFile()
{
    auto startDir = juce::File ("/Library/Audio/Plug-Ins/VST3");
    if (! startDir.isDirectory())
        startDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    fileChooser = std::make_unique<juce::FileChooser> (
        "Choisir un plugin instrument (.vst3 ou .component)",
        startDir, "*.vst3;*.component");

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles
               | juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File{})
            return;

        // On ferme un éventuel éditeur ouvert avant de changer d'instrument.
        pluginWindow = nullptr;

        auto error = engine.loadPluginFromFile (file);
        if (error.isNotEmpty())
            statusLabel.setText ("Erreur : " + error, juce::dontSendNotification);
    });
}

void MainComponent::showPluginEditor()
{
    auto* plugin = engine.getLoadedPlugin();
    if (plugin == nullptr)
    {
        statusLabel.setText ("Charge d'abord un plugin.", juce::dontSendNotification);
        return;
    }

    if (pluginWindow != nullptr)
    {
        pluginWindow->toFront (true);
        return;
    }

    pluginWindow = std::make_unique<PluginEditorWindow> (*plugin);
    pluginWindow->onCloseButton = [this] { pluginWindow = nullptr; };
}

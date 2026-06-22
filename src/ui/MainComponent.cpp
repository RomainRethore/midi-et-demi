#include "ui/MainComponent.h"

#include <cmath>

MainComponent::MainComponent()
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("Midi et demi - tempo & transport (etape 2)",
                        juce::dontSendNotification);
    titleLabel.setFont (juce::Font (20.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::topLeft);

    // --- transport ---
    addAndMakeVisible (playButton);
    playButton.onClick = [this] { togglePlay(); };

    addAndMakeVisible (bpmLabel);
    bpmLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (bpmSlider);
    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);
    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setValue (120.0, juce::dontSendNotification);
    bpmSlider.onValueChange = [this] { engine.setTempo (bpmSlider.getValue()); };
    engine.setTempo (bpmSlider.getValue());

    addAndMakeVisible (bpmUnitLabel);
    bpmUnitLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (metronomeToggle);
    metronomeToggle.setToggleState (true, juce::dontSendNotification);
    metronomeToggle.onClick = [this]
    {
        engine.setMetronomeEnabled (metronomeToggle.getToggleState());
    };
    engine.setMetronomeEnabled (true);

    addAndMakeVisible (positionLabel);
    positionLabel.setJustificationType (juce::Justification::centred);

    // --- plugin ---
    addAndMakeVisible (loadButton);
    loadButton.onClick = [this] { openPluginFile(); };

    addAndMakeVisible (editorButton);
    editorButton.onClick = [this] { showPluginEditor(); };

    addAndMakeVisible (keyboard);

    engine.start();
    startTimerHz (15); // rafraîchit statut + position

    setSize (820, 460);
}

MainComponent::~MainComponent()
{
    stopTimer();
    pluginWindow = nullptr; // ferme l'éditeur avant que l'engine ne disparaisse
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
    area.removeFromTop (8);

    auto transportRow = area.removeFromTop (40);
    playButton     .setBounds (transportRow.removeFromLeft (100).reduced (2));
    bpmLabel       .setBounds (transportRow.removeFromLeft (55).reduced (2));
    bpmSlider      .setBounds (transportRow.removeFromLeft (240).reduced (2));
    bpmUnitLabel   .setBounds (transportRow.removeFromLeft (45).reduced (2));
    metronomeToggle.setBounds (transportRow.removeFromLeft (120).reduced (2));
    positionLabel  .setBounds (transportRow.reduced (2));
    area.removeFromTop (8);

    auto pluginRow = area.removeFromTop (40);
    loadButton  .setBounds (pluginRow.removeFromLeft (200).reduced (2));
    editorButton.setBounds (pluginRow.removeFromLeft (200).reduced (2));

    keyboard.setBounds (area.removeFromBottom (140));
}

void MainComponent::timerCallback()
{
    statusLabel.setText (engine.getStatusText(), juce::dontSendNotification);

    const double beats     = engine.getPositionInBeats();
    const int    numerator = engine.getNumerator();
    const int    bar       = (int) std::floor (beats / numerator) + 1;
    const int    beatInBar = ((int) std::floor (beats)) % numerator + 1;

    positionLabel.setText (isPlaying ? ("Mesure " + juce::String (bar)
                                        + " - Temps " + juce::String (beatInBar))
                                     : juce::String ("Arrete"),
                           juce::dontSendNotification);
}

void MainComponent::togglePlay()
{
    isPlaying = ! isPlaying;
    engine.setPlaying (isPlaying);
    playButton.setButtonText (isPlaying ? "Stop" : "Lecture");
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

        pluginWindow = nullptr; // ferme l'ancien éditeur avant de changer d'instrument

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

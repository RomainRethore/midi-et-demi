#include "ui/MainComponent.h"

#include <cmath>

MainComponent::MainComponent()
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("Midi et demi - multipiste (etape 4a)", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (20.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::topLeft);

    // --- transport ---
    addAndMakeVisible (playButton);
    playButton.onClick = [this] { engine.setPlaying (! engine.isPlaying()); };

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
    metronomeToggle.onClick = [this] { engine.setMetronomeEnabled (metronomeToggle.getToggleState()); };
    engine.setMetronomeEnabled (true);

    addAndMakeVisible (positionLabel);
    positionLabel.setJustificationType (juce::Justification::centred);

    // --- sélecteur de piste ---
    addAndMakeVisible (tracksLabel);
    for (int i = 0; i < (int) trackButtons.size(); ++i)
    {
        auto& b = trackButtons[(size_t) i];
        addAndMakeVisible (b);
        b.setButtonText (juce::String (i + 1));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1000);
        b.onClick = [this, i] { selectTrack (i); };
    }
    trackButtons[0].setToggleState (true, juce::dontSendNotification);

    // --- contrôles de la piste active ---
    addAndMakeVisible (loadButton);
    loadButton.onClick = [this] { openPluginFile(); };

    addAndMakeVisible (editorButton);
    editorButton.onClick = [this] { showPluginEditor(); };

    addAndMakeVisible (barsLabel);
    barsLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (barsCombo);
    barsCombo.addItem ("1", 1);
    barsCombo.addItem ("2", 2);
    barsCombo.addItem ("4", 4);
    barsCombo.addItem ("8", 8);
    barsCombo.onChange = [this]
    {
        engine.setTrackBars (engine.getActiveTrack(), barsCombo.getSelectedId());
    };

    addAndMakeVisible (recordButton);
    recordButton.onClick = [this] { engine.pressRecord(); };

    addAndMakeVisible (clearButton);
    clearButton.onClick = [this] { engine.pressClear(); };

    addAndMakeVisible (volumeLabel);
    volumeLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (volumeSlider);
    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 24);
    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.onValueChange = [this]
    {
        engine.setTrackVolume (engine.getActiveTrack(), (float) volumeSlider.getValue());
    };

    addAndMakeVisible (muteToggle);
    muteToggle.onClick = [this]
    {
        engine.setTrackMute (engine.getActiveTrack(), muteToggle.getToggleState());
    };

    addAndMakeVisible (activeInfoLabel);
    activeInfoLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (keyboard);

    engine.start();
    selectTrack (0);
    startTimerHz (15);

    setSize (900, 560);
}

MainComponent::~MainComponent()
{
    stopTimer();
    pluginWindow = nullptr;
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (12);

    titleLabel.setBounds (area.removeFromTop (30));
    statusLabel.setBounds (area.removeFromTop (44));
    area.removeFromTop (6);

    auto transportRow = area.removeFromTop (38);
    playButton     .setBounds (transportRow.removeFromLeft (90).reduced (2));
    bpmLabel       .setBounds (transportRow.removeFromLeft (50).reduced (2));
    bpmSlider      .setBounds (transportRow.removeFromLeft (240).reduced (2));
    bpmUnitLabel   .setBounds (transportRow.removeFromLeft (40).reduced (2));
    metronomeToggle.setBounds (transportRow.removeFromLeft (120).reduced (2));
    positionLabel  .setBounds (transportRow.reduced (2));
    area.removeFromTop (8);

    auto trackRow = area.removeFromTop (36);
    tracksLabel.setBounds (trackRow.removeFromLeft (90).reduced (2));
    for (auto& b : trackButtons)
        b.setBounds (trackRow.removeFromLeft (42).reduced (2));
    area.removeFromTop (8);

    auto activeRow1 = area.removeFromTop (38);
    loadButton  .setBounds (activeRow1.removeFromLeft (180).reduced (2));
    editorButton.setBounds (activeRow1.removeFromLeft (90).reduced (2));
    barsLabel   .setBounds (activeRow1.removeFromLeft (70).reduced (2));
    barsCombo   .setBounds (activeRow1.removeFromLeft (60).reduced (2));
    recordButton.setBounds (activeRow1.removeFromLeft (140).reduced (2));
    clearButton .setBounds (activeRow1.removeFromLeft (100).reduced (2));
    area.removeFromTop (6);

    auto activeRow2 = area.removeFromTop (38);
    volumeLabel .setBounds (activeRow2.removeFromLeft (70).reduced (2));
    volumeSlider.setBounds (activeRow2.removeFromLeft (240).reduced (2));
    muteToggle  .setBounds (activeRow2.removeFromLeft (90).reduced (2));
    activeInfoLabel.setBounds (activeRow2.reduced (2));

    keyboard.setBounds (area.removeFromBottom (120));
}

void MainComponent::selectTrack (int index)
{
    engine.setActiveTrack (index);
    trackButtons[(size_t) index].setToggleState (true, juce::dontSendNotification);
    refreshActiveControls();
}

void MainComponent::refreshActiveControls()
{
    const int active = engine.getActiveTrack();
    barsCombo.setSelectedId (engine.getTrackBars (active), juce::dontSendNotification);
    volumeSlider.setValue (engine.getTrackVolume (active), juce::dontSendNotification);
    muteToggle.setToggleState (engine.isTrackMuted (active), juce::dontSendNotification);
}

void MainComponent::timerCallback()
{
    statusLabel.setText (engine.getStatusText(), juce::dontSendNotification);

    const bool   playing   = engine.isPlaying();
    const double beats     = engine.getPositionInBeats();
    const int    numerator = engine.getNumerator();
    const int    bar       = (int) std::floor (beats / numerator) + 1;
    const int    beatInBar = ((int) std::floor (beats)) % numerator + 1;

    playButton.setButtonText (playing ? "Stop" : "Lecture");
    positionLabel.setText (playing ? ("Mesure " + juce::String (bar)
                                      + " - Temps " + juce::String (beatInBar))
                                   : juce::String ("Arrete"),
                           juce::dontSendNotification);

    const int    active     = engine.getActiveTrack();
    const int    state      = engine.getTrackLoopState (active);
    const juce::String name = engine.getTrackPluginName (active);

    juce::String stateText;
    switch (state)
    {
        case 2:  stateText = "ENREGISTREMENT"; break;
        case 3:  stateText = "lecture";        break;
        default: stateText = "vide";           break;
    }

    activeInfoLabel.setText ("Piste " + juce::String (active + 1) + " - "
                             + (name.isNotEmpty() ? name : juce::String ("synthe sinus"))
                             + " - " + stateText,
                             juce::dontSendNotification);

    if (state == 2) // Recording
    {
        recordButton.setButtonText ("\xe2\x97\x8f REC");
        recordButton.setColour (juce::TextButton::buttonColourId, juce::Colours::red);
    }
    else
    {
        recordButton.setButtonText (state == 3 ? "Re-enregistrer" : "Enregistrer");
        recordButton.setColour (juce::TextButton::buttonColourId,
                                getLookAndFeel().findColour (juce::TextButton::buttonColourId));
    }

    // Couleur des pistes ayant une boucle.
    for (int i = 0; i < (int) trackButtons.size(); ++i)
    {
        const int st = engine.getTrackLoopState (i);
        auto colour = (st == 3) ? juce::Colours::green
                    : (st == 2) ? juce::Colours::red
                                : getLookAndFeel().findColour (juce::TextButton::buttonColourId);
        trackButtons[(size_t) i].setColour (juce::TextButton::buttonColourId, colour);
    }
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

        pluginWindow = nullptr;

        auto error = engine.loadPluginToActiveTrack (file);
        if (error.isNotEmpty())
            statusLabel.setText ("Erreur : " + error, juce::dontSendNotification);
    });
}

void MainComponent::showPluginEditor()
{
    auto* plugin = engine.getActivePlugin();
    if (plugin == nullptr)
    {
        statusLabel.setText ("Charge d'abord un plugin sur cette piste.", juce::dontSendNotification);
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

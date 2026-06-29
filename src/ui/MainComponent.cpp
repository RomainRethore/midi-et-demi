#include "ui/MainComponent.h"

#include <cmath>

MainComponent::MainComponent()
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("Midi et demi - sauvegarde de session (etape 8)", juce::dontSendNotification);
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
        b.onClick = [this, i] { selectTrack (i); };
    }

    addAndMakeVisible (sessionSaveButton);
    sessionSaveButton.onClick = [this]
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        fileChooser = std::make_unique<juce::FileChooser> (
            "Enregistrer la session", dir.getChildFile ("session.mdemi"), "*.mdemi");

        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File{})
                    return;
                if (f.getFileExtension().isEmpty())
                    f = f.withFileExtension ("mdemi");
                if (! engine.saveSession (f))
                    statusLabel.setText ("Echec de la sauvegarde", juce::dontSendNotification);
            });
    };

    addAndMakeVisible (sessionOpenButton);
    sessionOpenButton.onClick = [this]
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        fileChooser = std::make_unique<juce::FileChooser> (
            "Ouvrir une session", dir, "*.mdemi");

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File{})
                    return;
                pluginWindow = nullptr;        // un plugin de la session peut disparaître
                if (engine.loadSession (f))
                    selectTrack (0);            // rafraîchit les contrôles de la piste active
                else
                    statusLabel.setText ("Echec de l'ouverture", juce::dontSendNotification);
            });
    };

    // --- contrôles de la piste active ---
    addAndMakeVisible (loadButton);
    loadButton.onClick = [this] { openPluginFile(); };

    addAndMakeVisible (editorButton);
    editorButton.onClick = [this] { showPluginEditor(); };

    addAndMakeVisible (padsButton);
    padsButton.onClick = [this]
    {
        if (padWindow != nullptr) { padWindow->toFront (true); return; }
        padWindow = std::make_unique<PadWindow> (engine);
        padWindow->onCloseButton = [this] { padWindow = nullptr; };
    };

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

    addAndMakeVisible (undoButton);
    undoButton.onClick = [this] { engine.pressUndo(); };

    addAndMakeVisible (redoButton);
    redoButton.onClick = [this] { engine.pressRedo(); };

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

    addAndMakeVisible (mappingButton);
    mappingButton.onClick = [this]
    {
        if (mappingWindow != nullptr) { mappingWindow->toFront (true); return; }
        mappingWindow = std::make_unique<MappingWindow> (engine);
        mappingWindow->onCloseButton = [this] { mappingWindow = nullptr; };
    };

    addAndMakeVisible (activeInfoLabel);
    activeInfoLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (loopLanes);
    addAndMakeVisible (keyboard);

    engine.start();
    selectTrack (0);
    startTimerHz (30); // visu fluide de la tete de lecture

    setSize (920, 700);
}

MainComponent::~MainComponent()
{
    stopTimer();
    pluginWindow  = nullptr;
    mappingWindow = nullptr;
    padWindow     = nullptr;
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
    trackRow.removeFromLeft (16);
    sessionSaveButton.setBounds (trackRow.removeFromLeft (130).reduced (2));
    sessionOpenButton.setBounds (trackRow.removeFromLeft (130).reduced (2));
    area.removeFromTop (8);

    auto activeRow1 = area.removeFromTop (38);
    loadButton  .setBounds (activeRow1.removeFromLeft (160).reduced (2));
    editorButton.setBounds (activeRow1.removeFromLeft (80).reduced (2));
    padsButton  .setBounds (activeRow1.removeFromLeft (80).reduced (2));
    barsLabel   .setBounds (activeRow1.removeFromLeft (66).reduced (2));
    barsCombo   .setBounds (activeRow1.removeFromLeft (60).reduced (2));
    recordButton.setBounds (activeRow1.removeFromLeft (130).reduced (2));
    undoButton  .setBounds (activeRow1.removeFromLeft (90).reduced (2));
    redoButton  .setBounds (activeRow1.removeFromLeft (90).reduced (2));
    clearButton .setBounds (activeRow1.removeFromLeft (90).reduced (2));
    area.removeFromTop (6);

    auto activeRow2 = area.removeFromTop (38);
    volumeLabel .setBounds (activeRow2.removeFromLeft (70).reduced (2));
    volumeSlider.setBounds (activeRow2.removeFromLeft (240).reduced (2));
    muteToggle  .setBounds (activeRow2.removeFromLeft (90).reduced (2));
    mappingButton.setBounds (activeRow2.removeFromLeft (110).reduced (2));
    activeInfoLabel.setBounds (activeRow2.reduced (2));
    area.removeFromTop (8);

    keyboard.setBounds (area.removeFromBottom (110));
    area.removeFromBottom (8);
    loopLanes.setBounds (area); // occupe l'espace restant
}

void MainComponent::selectTrack (int index)
{
    engine.setActiveTrack (index);
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
    loopLanes.update();

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

    // Reflète les potards mappés (volume, BPM, mesures) sur l'UI.
    if (! volumeSlider.isMouseButtonDown())
        volumeSlider.setValue (engine.getTrackVolume (active), juce::dontSendNotification);
    if (! bpmSlider.isMouseButtonDown())
        bpmSlider.setValue (engine.getTempo(), juce::dontSendNotification);
    if (barsCombo.getSelectedId() != engine.getTrackBars (active))
        barsCombo.setSelectedId (engine.getTrackBars (active), juce::dontSendNotification);
    muteToggle.setToggleState (engine.isTrackMuted (active), juce::dontSendNotification);

    // Une action mappée a demandé d'ouvrir l'éditeur du plugin de la piste active.
    if (engine.consumeOpenEditorRequest())
        showPluginEditor();

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
        recordButton.setButtonText ("Stop REC");
        recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc62828));
    }
    else
    {
        recordButton.setButtonText (state == 3 ? "Overdub" : "Enregistrer");
        recordButton.setColour (juce::TextButton::buttonColourId,
                                getLookAndFeel().findColour (juce::TextButton::buttonColourId));
    }

    // Couleur = état de la boucle ; crochets = piste active.
    for (int i = 0; i < (int) trackButtons.size(); ++i)
    {
        const int st = engine.getTrackLoopState (i);
        const auto colour = (st == 2) ? juce::Colour (0xffc62828)   // rouge : enregistrement
                          : (st == 3) ? juce::Colour (0xff2e7d32)   // vert  : a une boucle
                                      : getLookAndFeel().findColour (juce::TextButton::buttonColourId);

        auto& b = trackButtons[(size_t) i];
        b.setColour (juce::TextButton::buttonColourId, colour);
        b.setButtonText (i == active ? ("[" + juce::String (i + 1) + "]")
                                     : juce::String (i + 1));
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

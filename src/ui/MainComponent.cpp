#include "ui/MainComponent.h"

#include <cmath>

MainComponent::MainComponent()
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("Midi et demi", juce::dontSendNotification);
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
    for (int i = 0; i < (int) strips.size(); ++i)
    {
        strips[(size_t) i] = std::make_unique<ChannelStrip> (engine, i);
        strips[(size_t) i]->onSelect = [this] (int idx) { selectTrack (idx); };
        addAndMakeVisible (*strips[(size_t) i]);
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

    addAndMakeVisible (exportButton);
    exportButton.onClick = [this]
    {
        if (engine.isCapturing()) { engine.stopCapture(); return; }

        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        fileChooser = std::make_unique<juce::FileChooser> (
            "Exporter le morceau en WAV", dir.getChildFile ("morceau.wav"), "*.wav");

        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File{})
                    return;
                if (f.getFileExtension().isEmpty())
                    f = f.withFileExtension ("wav");
                engine.startCapture (f);
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

    for (auto* gp : { &transportGroup, &instrumentGroup, &sessionGroup, &mixerGroup })
    {
        addAndMakeVisible (*gp);
        gp->setInterceptsMouseClicks (false, false); // cadre décoratif, laisse passer les clics
        gp->toBack();
    }
    transportGroup .setText ("Transport");
    instrumentGroup.setText ("Instrument (piste active)");
    sessionGroup   .setText ("Session / fichier");
    mixerGroup     .setText ("Pistes");

    engine.start();
    selectTrack (0);
    startTimerHz (30); // visu fluide de la tete de lecture

    setSize (1280, 820);
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
    constexpr int gap = 10;

    titleLabel.setBounds (area.removeFromTop (28));
    statusLabel.setBounds (area.removeFromTop (40));
    area.removeFromTop (gap);

    // --- Zone TRANSPORT (jouable) ---
    {
        auto zone = area.removeFromTop (62);
        transportGroup.setBounds (zone);
        auto r = zone.reduced (12, 18); // marge intérieure (sous le libellé du cadre)
        playButton     .setBounds (r.removeFromLeft (90).reduced (2));
        bpmLabel       .setBounds (r.removeFromLeft (50).reduced (2));
        bpmSlider      .setBounds (r.removeFromLeft (240).reduced (2));
        bpmUnitLabel   .setBounds (r.removeFromLeft (40).reduced (2));
        metronomeToggle.setBounds (r.removeFromLeft (120).reduced (2));
        positionLabel  .setBounds (r.reduced (2));
    }
    area.removeFromTop (gap);

    // --- Zone des blocs SOURIS : Instrument (gauche) + Session (droite) ---
    {
        auto zone = area.removeFromTop (70);
        auto left  = zone.removeFromLeft (zone.getWidth() / 2);
        auto right = zone;
        right.removeFromLeft (gap);

        instrumentGroup.setBounds (left);
        auto li = left.reduced (12, 18);
        loadButton  .setBounds (li.removeFromLeft (170).reduced (2));
        editorButton.setBounds (li.removeFromLeft (90).reduced (2));
        padsButton  .setBounds (li.removeFromLeft (90).reduced (2));
        mappingButton.setBounds (li.removeFromLeft (110).reduced (2));

        sessionGroup.setBounds (right);
        auto ri = right.reduced (12, 18);
        sessionSaveButton.setBounds (ri.removeFromLeft (140).reduced (2));
        sessionOpenButton.setBounds (ri.removeFromLeft (140).reduced (2));
        exportButton     .setBounds (ri.removeFromLeft (150).reduced (2));
    }
    area.removeFromTop (gap);

    // --- Zone MIXER : 8 voies + bandeau "piste active" ---
    {
        auto zone = area.removeFromTop (250);
        mixerGroup.setBounds (zone);
        auto m = zone.reduced (12, 18);

        // bandeau d'actions de la piste active (en bas de la zone)
        auto activeRow = m.removeFromBottom (34);
        barsLabel   .setBounds (activeRow.removeFromLeft (66).reduced (2));
        barsCombo   .setBounds (activeRow.removeFromLeft (60).reduced (2));
        recordButton.setBounds (activeRow.removeFromLeft (130).reduced (2));
        undoButton  .setBounds (activeRow.removeFromLeft (90).reduced (2));
        redoButton  .setBounds (activeRow.removeFromLeft (90).reduced (2));
        clearButton .setBounds (activeRow.removeFromLeft (90).reduced (2));
        activeInfoLabel.setBounds (activeRow.reduced (4, 2));
        m.removeFromBottom (6);

        // les 8 voies, réparties sur la largeur
        const int n = (int) strips.size();
        const int stripW = m.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            auto col = (i < n - 1) ? m.removeFromLeft (stripW) : m; // dernière voie = reste
            strips[(size_t) i]->setBounds (col.reduced (3));
        }
    }
    area.removeFromTop (gap);

    // --- Clavier (bas) + visu des boucles (reste) ---
    if (keyboard.isVisible())
    {
        keyboard.setBounds (area.removeFromBottom (110));
        area.removeFromBottom (gap);
    }
    loopLanes.setBounds (area);
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

    // Reflète les potards mappés (BPM, mesures) sur l'UI (volume/mute = voies).
    if (! bpmSlider.isMouseButtonDown())
        bpmSlider.setValue (engine.getTempo(), juce::dontSendNotification);
    if (barsCombo.getSelectedId() != engine.getTrackBars (active))
        barsCombo.setSelectedId (engine.getTrackBars (active), juce::dontSendNotification);

    // Mise à jour des voies de mixage.
    for (int i = 0; i < (int) strips.size(); ++i)
        strips[(size_t) i]->update (i == active);

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

    // Bouton export (capture WAV en cours ?)
    if (engine.isCapturing())
    {
        exportButton.setButtonText ("Stop export");
        exportButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc62828));
    }
    else
    {
        exportButton.setButtonText ("Exporter (.wav)");
        exportButton.setColour (juce::TextButton::buttonColourId,
                                getLookAndFeel().findColour (juce::TextButton::buttonColourId));
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

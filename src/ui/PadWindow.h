#pragma once

#include <JuceHeader.h>
#include <array>
#include "engine/AudioEngine.h"

namespace
{
    inline bool isAudioFile (const juce::String& path)
    {
        auto p = path.toLowerCase();
        return p.endsWith (".wav") || p.endsWith (".aif") || p.endsWith (".aiff")
            || p.endsWith (".flac") || p.endsWith (".ogg");
    }
}

/**
    Une ligne de pad : nom du pad (+ note), sample assigné, boutons Jouer /
    Charger / X, et **cible de glisser-déposer** (dépose un fichier audio dessus).
*/
class PadRow : public juce::Component,
               public juce::FileDragAndDropTarget
{
public:
    PadRow (AudioEngine& e, int padIndex) : engine (e), pad (padIndex)
    {
        addAndMakeVisible (name);
        addAndMakeVisible (sample);
        sample.setColour (juce::Label::backgroundColourId, juce::Colour (0xff20232c));

        addAndMakeVisible (playButton);
        playButton.setButtonText ("Jouer");
        playButton.onClick = [this] { engine.triggerActivePad (pad); };

        addAndMakeVisible (loadButton);
        loadButton.setButtonText ("Charger");
        loadButton.onClick = [this] { if (onChoose) onChoose (pad); };

        addAndMakeVisible (clearButton);
        clearButton.setButtonText ("X");
        clearButton.onClick = [this] { engine.clearActiveTrackSample (pad); };
    }

    std::function<void(int)> onChoose;

    void refresh (int baseNote)
    {
        name.setText ("Pad " + juce::String (pad + 1) + "  (note " + juce::String (baseNote + pad) + ")",
                      juce::dontSendNotification);
        const auto n = engine.getActiveTrackSampleName (pad);
        sample.setText (n.isNotEmpty() ? n : juce::String ("(vide)"), juce::dontSendNotification);
    }

    // --- FileDragAndDropTarget ---
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
            if (isAudioFile (f))
                return true;
        return false;
    }

    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true;  repaint(); }
    void fileDragExit  (const juce::StringArray&) override           { dragOver = false; repaint(); }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dragOver = false;
        repaint();
        for (auto& f : files)
            if (isAudioFile (f))
            {
                engine.loadSampleToActiveTrack (pad, juce::File (f));
                break; // un sample par pad
            }
    }

    void paint (juce::Graphics& g) override
    {
        if (dragOver)
        {
            g.setColour (juce::Colours::yellow.withAlpha (0.25f));
            g.fillRect (getLocalBounds());
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        name      .setBounds (r.removeFromLeft (130));
        clearButton.setBounds (r.removeFromRight (34));
        loadButton .setBounds (r.removeFromRight (84));
        playButton .setBounds (r.removeFromRight (70));
        sample     .setBounds (r);
    }

private:
    AudioEngine&     engine;
    int              pad;
    juce::Label      name, sample;
    juce::TextButton playButton, loadButton, clearButton;
    bool             dragOver = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadRow)
};

/** Panneau des 16 pads de la piste active. */
class PadContent : public juce::Component,
                   private juce::Timer
{
public:
    explicit PadContent (AudioEngine& e) : engine (e)
    {
        addAndMakeVisible (header);
        header.setText ("Depose un fichier audio sur un pad, ou charge-en plusieurs. "
                        "(Apercu : barre espace dans le selecteur, ou bouton Jouer.)",
                        juce::dontSendNotification);

        addAndMakeVisible (baseLabel);
        baseLabel.setText ("Note pad 1", juce::dontSendNotification);
        baseLabel.setJustificationType (juce::Justification::centredRight);

        addAndMakeVisible (baseSlider);
        baseSlider.setSliderStyle (juce::Slider::IncDecButtons);
        baseSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 24);
        baseSlider.setRange (0, 119, 1);
        baseSlider.setValue (engine.getActiveTrackPadBase(), juce::dontSendNotification);
        baseSlider.onValueChange = [this] { engine.setActiveTrackPadBase ((int) baseSlider.getValue()); };

        addAndMakeVisible (multiButton);
        multiButton.setButtonText ("Charger plusieurs...");
        multiButton.onClick = [this] { chooseMany(); };

        for (int p = 0; p < numPads; ++p)
        {
            rows[(size_t) p] = std::make_unique<PadRow> (engine, p);
            rows[(size_t) p]->onChoose = [this] (int pad) { chooseOne (pad); };
            list.addAndMakeVisible (*rows[(size_t) p]);
        }

        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        setSize (520, 20 + 30 + 32 + 4 + 12 * rowH);
        startTimerHz (8);
    }

    ~PadContent() override { stopTimer(); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        header.setBounds (area.removeFromTop (30));

        auto baseRow = area.removeFromTop (32);
        baseLabel  .setBounds (baseRow.removeFromLeft (90).reduced (2));
        baseSlider .setBounds (baseRow.removeFromLeft (160).reduced (2));
        multiButton.setBounds (baseRow.removeFromRight (180).reduced (2));
        area.removeFromTop (4);

        viewport.setBounds (area);
        const int w = viewport.getMaximumVisibleWidth();
        list.setSize (w, numPads * rowH);
        for (int p = 0; p < numPads; ++p)
            rows[(size_t) p]->setBounds (0, p * rowH, w, rowH);
    }

private:
    void timerCallback() override
    {
        baseSlider.setValue (engine.getActiveTrackPadBase(), juce::dontSendNotification);
        const int base = engine.getActiveTrackPadBase();
        for (auto& r : rows)
            r->refresh (base);
    }

    void chooseOne (int pad)
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Sample pour le pad " + juce::String (pad + 1),
            juce::File::getSpecialLocation (juce::File::userMusicDirectory),
            "*.wav;*.aif;*.aiff;*.flac;*.ogg");

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this, pad] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f != juce::File{})
                    engine.loadSampleToActiveTrack (pad, f);
            });
    }

    void chooseMany()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Choisir jusqu'a 16 samples (assignes dans l'ordre)",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory),
            "*.wav;*.aif;*.aiff;*.flac;*.ogg");

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
            [this] (const juce::FileChooser& fc)
            {
                auto files = fc.getResults();
                for (int i = 0; i < files.size() && i < numPads; ++i)
                    engine.loadSampleToActiveTrack (i, files[i]);
            });
    }

    static constexpr int numPads = 16;
    static constexpr int rowH    = 30;

    AudioEngine&   engine;
    juce::Label    header, baseLabel;
    juce::Slider   baseSlider;
    juce::TextButton multiButton;
    juce::Viewport viewport;
    juce::Component list;
    std::array<std::unique_ptr<PadRow>, numPads> rows;
    std::unique_ptr<juce::FileChooser> fileChooser;
};

/** Fenêtre flottante contenant le panneau des pads. */
class PadWindow : public juce::DocumentWindow
{
public:
    explicit PadWindow (AudioEngine& engine)
        : DocumentWindow ("Pads / samples", juce::Colours::darkgrey,
                          juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new PadContent (engine), true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    std::function<void()> onCloseButton;
    void closeButtonPressed() override { if (onCloseButton) onCloseButton(); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadWindow)
};

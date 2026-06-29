#pragma once

#include <JuceHeader.h>
#include <array>
#include "engine/AudioEngine.h"
#include "ui/SampleBrowser.h"

/**
    Une ligne de pad : nom (+ note), sample assigné, boutons Jouer / Charger / X.
    Cible de glisser-déposer DOUBLE : fichiers du Finder (FileDragAndDropTarget)
    ET glisser interne depuis le navigateur (DragAndDropTarget).
*/
class PadRow : public juce::Component,
               public juce::FileDragAndDropTarget,
               public juce::DragAndDropTarget
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

    void assign (const juce::String& path)
    {
        if (medui::isAudioFile (path))
            engine.loadSampleToActiveTrack (pad, juce::File (path));
    }

    // --- fichiers du Finder ---
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files) if (medui::isAudioFile (f)) return true;
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { setHover (true); }
    void fileDragExit  (const juce::StringArray&) override           { setHover (false); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        setHover (false);
        for (auto& f : files) if (medui::isAudioFile (f)) { assign (f); break; }
    }

    // --- glisser interne (navigateur) ---
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.isString(); }
    void itemDragEnter (const SourceDetails&) override { setHover (true); }
    void itemDragExit  (const SourceDetails&) override { setHover (false); }
    void itemDropped (const SourceDetails& d) override { setHover (false); assign (d.description.toString()); }

    void paint (juce::Graphics& g) override
    {
        if (hover)
        {
            g.setColour (juce::Colours::yellow.withAlpha (0.25f));
            g.fillRect (getLocalBounds());
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        name      .setBounds (r.removeFromLeft (128));
        clearButton.setBounds (r.removeFromRight (32));
        loadButton .setBounds (r.removeFromRight (80));
        playButton .setBounds (r.removeFromRight (64));
        sample     .setBounds (r);
    }

private:
    void setHover (bool h) { if (h != hover) { hover = h; repaint(); } }

    AudioEngine&     engine;
    int              pad;
    juce::Label      name, sample;
    juce::TextButton playButton, loadButton, clearButton;
    bool             hover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadRow)
};

/** Panneau des 16 pads (gauche) + navigateur de samples (droite). */
class PadContent : public juce::Component,
                   public juce::DragAndDropContainer,
                   private juce::Timer
{
public:
    explicit PadContent (AudioEngine& e) : engine (e)
    {
        addAndMakeVisible (header);
        header.setText ("Glisse un sample (Finder ou navigateur) sur un pad. "
                        "Clic dans le navigateur = ecoute.", juce::dontSendNotification);

        addAndMakeVisible (baseLabel);
        baseLabel.setText ("Note pad 1", juce::dontSendNotification);
        baseLabel.setJustificationType (juce::Justification::centredRight);

        addAndMakeVisible (baseSlider);
        baseSlider.setSliderStyle (juce::Slider::IncDecButtons);
        baseSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 24);
        baseSlider.setRange (0, 119, 1);
        baseSlider.setValue (engine.getActiveTrackPadBase(), juce::dontSendNotification);
        baseSlider.onValueChange = [this] { engine.setActiveTrackPadBase ((int) baseSlider.getValue()); };

        for (int p = 0; p < numPads; ++p)
        {
            rows[(size_t) p] = std::make_unique<PadRow> (engine, p);
            rows[(size_t) p]->onChoose = [this] (int pad) { chooseOne (pad); };
            list.addAndMakeVisible (*rows[(size_t) p]);
        }
        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        addAndMakeVisible (browser);

        setSize (900, 560);
        startTimerHz (8);
    }

    ~PadContent() override { stopTimer(); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        header.setBounds (area.removeFromTop (28));

        auto baseRow = area.removeFromTop (32);
        baseLabel  .setBounds (baseRow.removeFromLeft (90).reduced (2));
        baseSlider .setBounds (baseRow.removeFromLeft (160).reduced (2));
        area.removeFromTop (6);

        auto left = area.removeFromLeft (area.getWidth() / 2 - 5);
        area.removeFromLeft (10);
        auto right = area;

        viewport.setBounds (left);
        const int w = viewport.getMaximumVisibleWidth();
        list.setSize (w, numPads * rowH);
        for (int p = 0; p < numPads; ++p)
            rows[(size_t) p]->setBounds (0, p * rowH, w, rowH);

        browser.setBounds (right);
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

    static constexpr int numPads = 16;
    static constexpr int rowH    = 30;

    AudioEngine&     engine;
    juce::Label      header, baseLabel;
    juce::Slider     baseSlider;
    juce::Viewport   viewport;
    juce::Component  list;
    std::array<std::unique_ptr<PadRow>, numPads> rows;
    SampleBrowser    browser { engine };
    std::unique_ptr<juce::FileChooser> fileChooser;
};

/** Fenêtre flottante contenant le panneau des pads + navigateur. */
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

#pragma once

#include <JuceHeader.h>
#include <array>
#include "engine/AudioEngine.h"

/**
    Panneau des pads (16) de la PISTE ACTIVE : assignation d'un sample par pad,
    et réglage de la note du pad 1 (les pads = base..base+15).
*/
class PadContent : public juce::Component,
                   private juce::Timer
{
public:
    explicit PadContent (AudioEngine& e) : engine (e)
    {
        addAndMakeVisible (header);
        header.setText ("Charge un sample par pad (piste active). 'Note pad 1' = note MIDI du 1er pad.",
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

        for (int p = 0; p < numPads; ++p)
        {
            auto& row = rows[(size_t) p];
            list.addAndMakeVisible (row.name);
            list.addAndMakeVisible (row.sample);
            row.sample.setColour (juce::Label::backgroundColourId, juce::Colour (0xff20232c));
            list.addAndMakeVisible (row.load);
            row.load.setButtonText ("Charger");
            row.load.onClick = [this, p] { chooseSample (p); };
            list.addAndMakeVisible (row.clear);
            row.clear.setButtonText ("X");
            row.clear.onClick = [this, p] { engine.clearActiveTrackSample (p); };
        }

        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        setSize (480, 20 + 30 + 32 + 4 + 12 * rowH);
        startTimerHz (8);
    }

    ~PadContent() override { stopTimer(); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        header.setBounds (area.removeFromTop (30));

        auto baseRow = area.removeFromTop (32);
        baseLabel.setBounds (baseRow.removeFromLeft (90).reduced (2));
        baseSlider.setBounds (baseRow.removeFromLeft (160).reduced (2));
        area.removeFromTop (4);

        viewport.setBounds (area);
        const int w = viewport.getMaximumVisibleWidth();
        list.setSize (w, numPads * rowH);
        for (int p = 0; p < numPads; ++p)
        {
            auto& row = rows[(size_t) p];
            juce::Rectangle<int> r (0, p * rowH, w, rowH);
            r.reduce (2, 2);
            row.name  .setBounds (r.removeFromLeft (120));
            row.clear .setBounds (r.removeFromRight (34));
            row.load  .setBounds (r.removeFromRight (90));
            row.sample.setBounds (r);
        }
    }

private:
    void timerCallback() override
    {
        baseSlider.setValue (engine.getActiveTrackPadBase(), juce::dontSendNotification);
        const int base = engine.getActiveTrackPadBase();
        for (int p = 0; p < numPads; ++p)
        {
            rows[(size_t) p].name.setText ("Pad " + juce::String (p + 1)
                                           + "  (note " + juce::String (base + p) + ")",
                                           juce::dontSendNotification);
            const auto n = engine.getActiveTrackSampleName (p);
            rows[(size_t) p].sample.setText (n.isNotEmpty() ? n : juce::String ("(vide)"),
                                             juce::dontSendNotification);
        }
    }

    void chooseSample (int pad)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        fileChooser = std::make_unique<juce::FileChooser> (
            "Choisir un sample pour le pad " + juce::String (pad + 1),
            dir, "*.wav;*.aif;*.aiff;*.flac;*.ogg");

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this, pad] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File{})
                    return;
                auto err = engine.loadSampleToActiveTrack (pad, f);
                if (err.isNotEmpty())
                    header.setText ("Erreur : " + err, juce::dontSendNotification);
            });
    }

    struct Row
    {
        juce::Label      name;
        juce::Label      sample;
        juce::TextButton load;
        juce::TextButton clear;
    };

    static constexpr int numPads = 16;
    static constexpr int rowH    = 30;

    AudioEngine&   engine;
    juce::Label    header, baseLabel;
    juce::Slider   baseSlider;
    juce::Viewport viewport;
    juce::Component list;
    std::array<Row, numPads> rows;
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

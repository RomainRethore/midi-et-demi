#pragma once

#include <JuceHeader.h>
#include <array>
#include "engine/AudioEngine.h"

/**
    Contenu de la fenêtre de mapping : en-tête + moniteur MIDI (fixes), puis une
    liste défilante (une ligne par action) avec l'association courante, un bouton
    « Apprendre » et un bouton pour effacer.
*/
class MappingContent : public juce::Component,
                       private juce::Timer
{
public:
    explicit MappingContent (AudioEngine& e) : engine (e)
    {
        addAndMakeVisible (header);
        header.setText ("Clique \"Apprendre\" puis bouge un potard / bouton (CC) de l'Oxygen.",
                        juce::dontSendNotification);
        header.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (monitor);
        monitor.setJustificationType (juce::Justification::centredLeft);
        monitor.setColour (juce::Label::backgroundColourId, juce::Colour (0xff20232c));
        monitor.setColour (juce::Label::textColourId, juce::Colours::yellow);

        for (int s = 0; s < numSlots; ++s)
        {
            auto& row = rows[(size_t) s];

            list.addAndMakeVisible (row.name);
            row.name.setText (slotNames[(size_t) s], juce::dontSendNotification);

            list.addAndMakeVisible (row.binding);
            row.binding.setJustificationType (juce::Justification::centred);
            row.binding.setColour (juce::Label::backgroundColourId, juce::Colour (0xff20232c));

            list.addAndMakeVisible (row.learn);
            row.learn.setButtonText ("Apprendre");
            row.learn.onClick = [this, s]
            {
                if (engine.getLearnSlot() == s) engine.cancelLearn();
                else                            engine.startLearn (s);
            };

            list.addAndMakeVisible (row.clear);
            row.clear.setButtonText ("X");
            row.clear.onClick = [this, s] { engine.clearBinding (s); };
        }

        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        setSize (520, 20 + 32 + 26 + 4 + visibleRows * rowH);
        startTimerHz (15);
    }

    ~MappingContent() override { stopTimer(); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        header.setBounds (area.removeFromTop (32));
        monitor.setBounds (area.removeFromTop (26));
        area.removeFromTop (4);

        viewport.setBounds (area);

        const int w = viewport.getMaximumVisibleWidth();
        list.setSize (w, numSlots * rowH);

        for (int s = 0; s < numSlots; ++s)
        {
            auto& row = rows[(size_t) s];
            juce::Rectangle<int> r (0, s * rowH, w, rowH);
            r.reduce (2, 2);
            row.name   .setBounds (r.removeFromLeft (180));
            row.binding.setBounds (r.removeFromLeft (90));
            row.learn  .setBounds (r.removeFromLeft (110));
            row.clear  .setBounds (r.removeFromLeft (40));
        }
    }

private:
    void timerCallback() override
    {
        const int code = engine.getLastMidiCode();
        const int val  = engine.getLastMidiValue();
        monitor.setText (code < 0     ? juce::String ("Dernier recu : -")
                       : code >= 1000 ? "Dernier recu : CC " + juce::String (code - 1000)
                                        + " = " + juce::String (val)
                                      : "Dernier recu : Note " + juce::String (code)
                                        + " (vel " + juce::String (val) + ")",
                         juce::dontSendNotification);

        const int learning = engine.getLearnSlot();
        for (int s = 0; s < numSlots; ++s)
        {
            const int code2 = engine.getBindingCode (s);
            juce::String t = code2 < 0     ? juce::String ("-")
                           : code2 >= 1000 ? "CC " + juce::String (code2 - 1000)
                                           : "Note " + juce::String (code2);
            rows[(size_t) s].binding.setText (t, juce::dontSendNotification);
            rows[(size_t) s].learn.setButtonText (learning == s ? "...ecoute" : "Apprendre");
        }
    }

    struct Row
    {
        juce::Label      name;
        juce::Label      binding;
        juce::TextButton learn;
        juce::TextButton clear;
    };

    static constexpr int numSlots    = 19;
    static constexpr int rowH        = 30;
    static constexpr int visibleRows = 12; // au-delà : défilement

    AudioEngine&   engine;
    juce::Label    header;
    juce::Label    monitor;
    juce::Viewport viewport;
    juce::Component list;
    std::array<Row, numSlots> rows;

    const std::array<const char*, numSlots> slotNames {
        "Lecture / Stop", "Enregistrer", "Effacer", "Annuler (undo)", "Refaire (redo)",
        "Metronome", "Volume (piste active)", "Selecteur de piste (potard)",
        "Mesures (piste active)", "BPM (tempo)",
        "Volume piste 1", "Volume piste 2", "Volume piste 3", "Volume piste 4",
        "Volume piste 5", "Volume piste 6", "Volume piste 7", "Volume piste 8",
        "Editeur (piste active)"
    };
};

/** Fenêtre flottante contenant le panneau de mapping. */
class MappingWindow : public juce::DocumentWindow
{
public:
    explicit MappingWindow (AudioEngine& engine)
        : DocumentWindow ("Mapping MIDI", juce::Colours::darkgrey,
                          juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MappingContent (engine), true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    std::function<void()> onCloseButton;
    void closeButtonPressed() override { if (onCloseButton) onCloseButton(); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MappingWindow)
};

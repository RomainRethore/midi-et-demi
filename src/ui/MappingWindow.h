#pragma once

#include <JuceHeader.h>
#include <array>
#include "engine/AudioEngine.h"

/**
    Contenu de la fenêtre de mapping : une ligne par action, avec l'association
    MIDI courante, un bouton « Apprendre » et un bouton pour effacer.
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

            addAndMakeVisible (row.name);
            row.name.setText (slotNames[(size_t) s], juce::dontSendNotification);

            addAndMakeVisible (row.binding);
            row.binding.setJustificationType (juce::Justification::centred);
            row.binding.setColour (juce::Label::backgroundColourId, juce::Colour (0xff20232c));

            addAndMakeVisible (row.learn);
            row.learn.setButtonText ("Apprendre");
            row.learn.onClick = [this, s]
            {
                if (engine.getLearnSlot() == s) engine.cancelLearn();
                else                            engine.startLearn (s);
            };

            addAndMakeVisible (row.clear);
            row.clear.setButtonText ("X");
            row.clear.onClick = [this, s] { engine.clearBinding (s); };
        }

        setSize (500, 76 + numSlots * 30);
        startTimerHz (15);
    }

    ~MappingContent() override { stopTimer(); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        header.setBounds (area.removeFromTop (32));
        monitor.setBounds (area.removeFromTop (26));
        area.removeFromTop (4);

        for (int s = 0; s < numSlots; ++s)
        {
            auto& row = rows[(size_t) s];
            auto r = area.removeFromTop (30);
            row.name   .setBounds (r.removeFromLeft (180).reduced (2));
            row.binding.setBounds (r.removeFromLeft (90).reduced (2));
            row.learn  .setBounds (r.removeFromLeft (110).reduced (2));
            row.clear  .setBounds (r.removeFromLeft (40).reduced (2));
        }
    }

private:
    void timerCallback() override
    {
        // moniteur : dernier contrôle reçu
        const int code = engine.getLastMidiCode();
        const int val  = engine.getLastMidiValue();
        monitor.setText (code < 0   ? juce::String ("Dernier recu : -")
                       : code >= 1000 ? "Dernier recu : CC " + juce::String (code - 1000)
                                        + " = " + juce::String (val)
                                      : "Dernier recu : Note " + juce::String (code)
                                        + " (vel " + juce::String (val) + ")",
                         juce::dontSendNotification);

        const int learning = engine.getLearnSlot();

        for (int s = 0; s < numSlots; ++s)
        {
            const int code = engine.getBindingCode (s);
            juce::String t = code < 0 ? juce::String ("-")
                           : code >= 1000 ? "CC " + juce::String (code - 1000)
                                          : "Note " + juce::String (code);
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

    static constexpr int numSlots = 11;

    AudioEngine& engine;
    juce::Label  header;
    juce::Label  monitor;
    std::array<Row, numSlots> rows;

    const std::array<const char*, numSlots> slotNames {
        "Lecture / Stop", "Enregistrer", "Effacer", "Annuler passe", "Metronome",
        "Volume (piste active)", "Selecteur de piste (potard)",
        "Piste suivante", "Piste precedente",
        "Mesures (piste active)", "BPM (tempo)"
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

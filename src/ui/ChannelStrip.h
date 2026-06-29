#pragma once

#include <JuceHeader.h>
#include "engine/AudioEngine.h"

/**
    Une voie de la table de mixage : numéro/sélection, nom de l'instrument,
    fader de volume vertical, mute, et couleur de fond selon l'état de la boucle.
*/
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (AudioEngine& e, int idx) : engine (e), index (idx)
    {
        addAndMakeVisible (selectButton);
        selectButton.setButtonText (juce::String (index + 1));
        selectButton.onClick = [this] { if (onSelect) onSelect (index); };

        addAndMakeVisible (nameLabel);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setFont (juce::Font (11.0f));
        nameLabel.setMinimumHorizontalScale (0.5f);

        addAndMakeVisible (volumeSlider);
        volumeSlider.setSliderStyle (juce::Slider::LinearVertical);
        volumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volumeSlider.setRange (0.0, 1.0, 0.01);
        volumeSlider.onValueChange = [this]
        {
            engine.setTrackVolume (index, (float) volumeSlider.getValue());
        };

        addAndMakeVisible (muteButton);
        muteButton.setButtonText ("Mute");
        muteButton.setClickingTogglesState (true);
        muteButton.onClick = [this]
        {
            engine.setTrackMute (index, muteButton.getToggleState());
        };
    }

    std::function<void(int)> onSelect;

    /** Rafraîchit l'affichage (appelé par le timer de l'UI). */
    void update (bool isActive)
    {
        const int st = engine.getTrackLoopState (index);
        if (st != loopState || isActive != active)
        {
            loopState = st;
            active = isActive;
            repaint();
        }

        nameLabel.setText (engine.getTrackInstrumentName (index), juce::dontSendNotification);
        if (! volumeSlider.isMouseButtonDown())
            volumeSlider.setValue (engine.getTrackVolume (index), juce::dontSendNotification);
        muteButton.setToggleState (engine.isTrackMuted (index), juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const auto bg = loopState == 2 ? juce::Colour (0xff5a2233)   // rouge sombre : REC
                      : loopState == 3 ? juce::Colour (0xff22432a)   // vert sombre : boucle
                                       : juce::Colour (0xff262a33);  // gris : vide
        g.setColour (bg);
        g.fillRoundedRectangle (r, 4.0f);

        if (active)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawRoundedRectangle (r, 4.0f, 2.0f);
        }
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (4);
        selectButton.setBounds (a.removeFromTop (26));
        a.removeFromTop (2);
        nameLabel.setBounds (a.removeFromTop (16));
        muteButton.setBounds (a.removeFromBottom (24));
        a.removeFromBottom (2);
        volumeSlider.setBounds (a);
    }

private:
    AudioEngine&       engine;
    int                index;
    juce::TextButton   selectButton;
    juce::Label        nameLabel;
    juce::Slider       volumeSlider;
    juce::TextButton   muteButton;
    bool active = false;
    int  loopState = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

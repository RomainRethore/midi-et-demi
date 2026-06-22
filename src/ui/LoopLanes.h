#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>
#include "engine/AudioEngine.h"
#include "domain/LoopClip.h"

/**
    Visualisation des 8 boucles : un couloir par piste, les notes en petits
    blocs (position = temps, hauteur = note), et une tête de lecture qui avance.
    L'UI copie les événements à chaque rafraîchissement (lecture sûre car le
    tableau de la boucle ne se réalloue jamais — cf. LoopClip).
*/
class LoopLanes : public juce::Component
{
public:
    explicit LoopLanes (AudioEngine& e) : engine (e) {}

    /** À appeler depuis le timer de l'UI : rafraîchit les copies et redessine. */
    void update()
    {
        posBeats = engine.getPositionInBeats();
        playing  = engine.isPlaying();
        active   = engine.getActiveTrack();

        for (int i = 0; i < numLanes; ++i)
            engine.getTrackDisplay (i, snapshots[(size_t) i], lengths[(size_t) i]);

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        const float laneH = area.getHeight() / (float) numLanes;

        for (int i = 0; i < numLanes; ++i)
        {
            juce::Rectangle<float> lane (area.getX(), area.getY() + (float) i * laneH,
                                         area.getWidth(), laneH);
            auto inner = lane.reduced (1.0f);

            g.setColour (i == active ? juce::Colour (0xff33384a)
                                     : juce::Colour (0xff20232c));
            g.fillRect (inner);

            // numéro de piste
            auto labelArea = inner.removeFromLeft (22.0f);
            g.setColour (juce::Colours::grey);
            g.setFont (12.0f);
            g.drawText (juce::String (i + 1), labelArea.toNearestInt(),
                        juce::Justification::centred);

            const double L = lengths[(size_t) i];

            if (L <= 0.0)
            {
                g.setColour (juce::Colours::dimgrey);
                g.setFont (11.0f);
                g.drawText ("(vide)", inner.toNearestInt(), juce::Justification::centredLeft);
            }
            else
            {
                // notes (note-on uniquement)
                g.setColour (juce::Colour (0xff5fd75f));
                for (const auto& ev : snapshots[(size_t) i])
                {
                    const bool noteOn = ev.numBytes >= 3
                                     && (ev.bytes[0] & 0xF0) == 0x90
                                     && ev.bytes[2] > 0;
                    if (! noteOn)
                        continue;

                    const float fx = (float) (ev.beat / L);
                    const float x  = inner.getX() + fx * inner.getWidth();
                    const float fy = 1.0f - (float) ev.bytes[1] / 127.0f;
                    const float y  = inner.getY() + 2.0f + fy * (inner.getHeight() - 4.0f);
                    g.fillRect (x, y, 2.5f, 2.5f);
                }

                // tête de lecture
                if (playing)
                {
                    const double frac = std::fmod (posBeats, L) / L;
                    const float  x    = inner.getX() + (float) frac * inner.getWidth();
                    g.setColour (juce::Colours::yellow.withAlpha (0.85f));
                    g.fillRect (x, inner.getY(), 1.5f, inner.getHeight());
                }
            }
        }
    }

private:
    static constexpr int numLanes = 8;

    AudioEngine& engine;
    std::array<std::vector<med::ClipEvent>, numLanes> snapshots;
    std::array<double, numLanes> lengths {};
    double posBeats = 0.0;
    bool   playing  = false;
    int    active   = 0;
};

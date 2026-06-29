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

    /** Appelé quand on clique un couloir (pour sélectionner la piste). */
    std::function<void(int)> onSelectLane;

    void mouseDown (const juce::MouseEvent& e) override
    {
        const float laneH = (float) getHeight() / (float) numLanes;
        if (laneH > 0.0f && onSelectLane)
            onSelectLane (juce::jlimit (0, numLanes - 1, (int) (e.position.y / laneH)));
    }

    /** À appeler depuis le timer de l'UI : rafraîchit les copies et redessine. */
    void update()
    {
        posBeats  = engine.getPositionInBeats();
        playing   = engine.isPlaying();
        active    = engine.getActiveTrack();
        numerator = engine.getNumerator();

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
                // grille : lignes de temps (faibles) + lignes de mesure (marquées)
                const int totalBeats = (int) std::lround (L);
                for (int b = 1; b < totalBeats; ++b)
                {
                    const float gx      = inner.getX() + (float) ((double) b / L) * inner.getWidth();
                    const bool  barLine = (numerator > 0) && (b % numerator == 0);
                    g.setColour (barLine ? juce::Colours::white.withAlpha (0.28f)
                                         : juce::Colours::white.withAlpha (0.08f));
                    g.fillRect (gx, inner.getY(), barLine ? 1.5f : 1.0f, inner.getHeight());
                }

                // notes : un rectangle par note (longueur = durée)
                g.setColour (juce::Colour (0xff5fd75f));
                const float w     = inner.getWidth();
                const float noteH = juce::jmax (6.0f, inner.getHeight() / 10.0f);

                for (const auto& n : snapshots[(size_t) i])
                {
                    const double dur = n.lengthBeats;
                    const float  fy  = 1.0f - (float) n.pitch / 127.0f;
                    const float  y   = inner.getY() + 2.0f + fy * (inner.getHeight() - noteH - 4.0f);
                    const float  x1  = inner.getX() + (float) (n.startBeat / L) * w;

                    if (n.startBeat + dur <= L)
                    {
                        g.fillRect (x1, y, juce::jmax (2.0f, (float) (dur / L) * w), noteH);
                    }
                    else // note tenue à cheval sur la fin : deux segments
                    {
                        g.fillRect (x1, y, juce::jmax (2.0f, (float) ((L - n.startBeat) / L) * w), noteH);
                        g.fillRect (inner.getX(), y,
                                    juce::jmax (2.0f, (float) ((n.startBeat + dur - L) / L) * w), noteH);
                    }
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
    std::array<std::vector<med::Note>, numLanes> snapshots;
    std::array<double, numLanes> lengths {};
    double posBeats  = 0.0;
    bool   playing   = false;
    int    active    = 0;
    int    numerator = 4;
};

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
                // notes : rectangles dont la longueur = durée (note-on -> note-off)
                g.setColour (juce::Colour (0xff5fd75f));
                const auto&  evs = snapshots[(size_t) i];
                const float  w   = inner.getWidth();
                const float  noteH = juce::jmax (3.0f, inner.getHeight() / 24.0f);

                for (const auto& on : evs)
                {
                    const bool isOn = on.numBytes >= 3
                                   && (on.bytes[0] & 0xF0) == 0x90
                                   && on.bytes[2] > 0;
                    if (! isOn)
                        continue;

                    const int ch    = on.bytes[0] & 0x0F;
                    const int pitch = on.bytes[1];

                    // Cherche le note-off correspondant le plus proche (circulaire).
                    double dur = -1.0;
                    for (const auto& off : evs)
                    {
                        const bool isOff = off.numBytes >= 2
                                        && (((off.bytes[0] & 0xF0) == 0x80)
                                            || ((off.bytes[0] & 0xF0) == 0x90 && off.bytes[2] == 0));
                        if (! isOff || (off.bytes[0] & 0x0F) != ch || off.bytes[1] != pitch)
                            continue;

                        double d = std::fmod (off.beat - on.beat, L);
                        if (d < 0.0)    d += L;
                        if (d <= 1.0e-6) d = L;
                        if (dur < 0.0 || d < dur)
                            dur = d;
                    }
                    if (dur < 0.0)
                        dur = 0.25; // pas de note-off trouvé : durée par défaut

                    const float fy = 1.0f - (float) pitch / 127.0f;
                    const float y  = inner.getY() + 2.0f + fy * (inner.getHeight() - noteH - 4.0f);
                    const float x1 = inner.getX() + (float) (on.beat / L) * w;

                    if (on.beat + dur <= L)
                    {
                        g.fillRect (x1, y, juce::jmax (2.0f, (float) (dur / L) * w), noteH);
                    }
                    else // note tenue à cheval sur la fin : deux segments
                    {
                        g.fillRect (x1, y, juce::jmax (2.0f, (float) ((L - on.beat) / L) * w), noteH);
                        g.fillRect (inner.getX(), y,
                                    juce::jmax (2.0f, (float) ((on.beat + dur - L) / L) * w), noteH);
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
    std::array<std::vector<med::ClipEvent>, numLanes> snapshots;
    std::array<double, numLanes> lengths {};
    double posBeats = 0.0;
    bool   playing  = false;
    int    active   = 0;
};

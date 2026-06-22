#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace med // "midi et demi"
{

/** Un événement MIDI enregistré, positionné en TEMPS musical (battements)
    dans la boucle. Stocker en temps (et non en samples) rend la boucle
    indépendante du tempo : changer le BPM ne casse pas le calage. */
struct ClipEvent
{
    double  beat = 0.0;          // position dans la boucle, [0, lengthBeats)
    uint8_t bytes[3] = { 0, 0, 0 };
    int     numBytes = 0;
};

/**
    Boucle MIDI PURE (sans JUCE) : une longueur (en temps) + une liste
    d'événements. Sa logique de fenêtrage est testable isolément.
*/
class LoopClip
{
public:
    void   setLengthBeats (double l) noexcept { lengthBeats = (l > 0.0 ? l : 0.0); }
    double getLengthBeats() const noexcept    { return lengthBeats; }

    void reserve (std::size_t n) { events.reserve (n); }
    void clear() noexcept        { events.clear(); }
    bool isEmpty() const noexcept { return events.empty(); }
    std::size_t size() const noexcept { return events.size(); }

    void addEvent (double beat, const uint8_t* data, int n)
    {
        ClipEvent e;
        e.beat     = beat;
        e.numBytes = (n > 3 ? 3 : (n < 0 ? 0 : n));
        for (int i = 0; i < e.numBytes; ++i)
            e.bytes[i] = data[i];
        events.push_back (e);
    }

    /** Appelle fn(event, offsetBeats) pour chaque événement dont la position
        est dans [fromBeat, toBeat). offsetBeats = event.beat - fromBeat.
        Ne gère PAS le bouclage : l'appelant découpe la fenêtre si besoin. */
    template <class Fn>
    void forEachInWindow (double fromBeat, double toBeat, Fn&& fn) const
    {
        for (const auto& e : events)
            if (e.beat >= fromBeat && e.beat < toBeat)
                fn (e, e.beat - fromBeat);
    }

private:
    double                 lengthBeats = 0.0;
    std::vector<ClipEvent> events;
};

} // namespace med

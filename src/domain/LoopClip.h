#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace med // "midi et demi"
{

/** Un événement MIDI enregistré, positionné en TEMPS musical (battements). */
struct ClipEvent
{
    double  beat = 0.0;          // position dans la boucle, [0, lengthBeats)
    uint8_t bytes[3] = { 0, 0, 0 };
    int     numBytes = 0;
};

/**
    Boucle MIDI PURE (sans JUCE).

    Particularité pour l'undo/redo : on distingue le tableau de stockage
    (`events`) d'un **compteur logique** (`usedCount`). truncate() baisse le
    compteur sans détruire les données => restore() peut les remettre (redo).
    Ajouter une note après un undo écrase l'historique de redo (branche
    abandonnée). Aucune réallocation sur le chemin audio (capacité fixe).
*/
class LoopClip
{
public:
    static constexpr std::size_t maxEvents = 50000;

    void   setLengthBeats (double l) noexcept { lengthBeats = (l > 0.0 ? l : 0.0); }
    double getLengthBeats() const noexcept    { return lengthBeats; }

    void reserve (std::size_t n) { events.reserve (n); }

    void clear() noexcept { events.clear(); usedCount = 0; }
    bool isEmpty() const noexcept { return usedCount == 0; }
    std::size_t size() const noexcept { return usedCount; }
    std::size_t getUsedCount() const noexcept { return usedCount; }

    void addEvent (double beat, const uint8_t* data, int n)
    {
        if (usedCount >= maxEvents)
            return;

        ClipEvent e;
        e.beat     = beat;
        e.numBytes = (n > 3 ? 3 : (n < 0 ? 0 : n));
        for (int i = 0; i < e.numBytes; ++i)
            e.bytes[i] = data[i];

        if (usedCount < events.size())
            events[usedCount] = e;     // réécrit un emplacement libéré par un undo
        else
            events.push_back (e);

        ++usedCount;

        if (events.size() > usedCount) // une nouvelle note invalide la redo
            events.resize (usedCount);
    }

    /** Undo : baisse le compteur logique, garde les données pour un éventuel redo. */
    void truncate (std::size_t n) noexcept { if (n < usedCount) usedCount = n; }

    /** Redo : remonte le compteur (dans la limite des données conservées). */
    void restore (std::size_t n) noexcept { usedCount = (n <= events.size() ? n : events.size()); }

    const std::vector<ClipEvent>& getEvents() const noexcept { return events; }

    /** fn(event, offsetBeats) pour les événements logiques dans [fromBeat, toBeat). */
    template <class Fn>
    void forEachInWindow (double fromBeat, double toBeat, Fn&& fn) const
    {
        for (std::size_t i = 0; i < usedCount; ++i)
        {
            const auto& e = events[i];
            if (e.beat >= fromBeat && e.beat < toBeat)
                fn (e, e.beat - fromBeat);
        }
    }

private:
    double                 lengthBeats = 0.0;
    std::vector<ClipEvent> events;
    std::size_t            usedCount = 0;
};

} // namespace med

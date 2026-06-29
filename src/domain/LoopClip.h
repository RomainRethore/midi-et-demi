#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <cmath>

namespace med // "midi et demi"
{

/** Une note enregistrée, en TEMPS musical (battements). channel = 1..16. */
struct Note
{
    double  startBeat   = 0.0;
    double  lengthBeats = 0.0;
    uint8_t channel     = 1;
    uint8_t pitch       = 0;
    uint8_t velocity    = 0;
};

/**
    Boucle MIDI PURE (sans JUCE), stockée PAR NOTE.

    Undo/redo par note : un compteur logique `usedCount` ; truncate() le baisse
    (sans détruire les données -> redo via restore()). Ajouter une note après un
    undo abandonne la branche de redo. Aucune réallocation sur le chemin audio.
*/
class LoopClip
{
public:
    static constexpr std::size_t maxNotes = 20000;

    void   setLengthBeats (double l) noexcept { lengthBeats = (l > 0.0 ? l : 0.0); }
    double getLengthBeats() const noexcept    { return lengthBeats; }

    void reserve (std::size_t n) { notes.reserve (n); }

    void clear() noexcept { notes.clear(); usedCount = 0; }
    bool isEmpty() const noexcept { return usedCount == 0; }
    std::size_t size() const noexcept { return usedCount; }
    std::size_t getUsedCount() const noexcept { return usedCount; }

    void addNote (const Note& n)
    {
        if (usedCount >= maxNotes)
            return;

        if (usedCount < notes.size())
            notes[usedCount] = n;       // réécrit un emplacement libéré par un undo
        else
            notes.push_back (n);

        ++usedCount;

        if (notes.size() > usedCount)   // une nouvelle note invalide la redo
            notes.resize (usedCount);
    }

    /** Undo : baisse le compteur logique (garde les données pour un redo). */
    void truncate (std::size_t n) noexcept { if (n < usedCount) usedCount = n; }

    /** Redo : remonte le compteur, dans la limite des données conservées. */
    void restore (std::size_t n) noexcept { usedCount = (n <= notes.size() ? n : notes.size()); }

    const std::vector<Note>& getNotes() const noexcept { return notes; }

    /** Émet les note-on / note-off tombant dans la fenêtre [from, to) (sans
        bouclage ; l'appelant découpe la fenêtre). offFn reçoit l'instant de
        relâchement, qui peut tomber ailleurs dans la boucle que le début. */
    template <class OnFn, class OffFn>
    void emitWindow (double from, double to, OnFn&& onFn, OffFn&& offFn) const
    {
        if (lengthBeats <= 0.0)
            return;

        for (std::size_t i = 0; i < usedCount; ++i)
        {
            const auto& n = notes[i];

            if (n.startBeat >= from && n.startBeat < to)
                onFn (n, n.startBeat - from);

            const double off = std::fmod (n.startBeat + n.lengthBeats, lengthBeats);
            if (off >= from && off < to)
                offFn (n, off - from);
        }
    }

private:
    double            lengthBeats = 0.0;
    std::vector<Note> notes;
    std::size_t       usedCount = 0;
};

} // namespace med

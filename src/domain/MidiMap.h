#pragma once

#include <array>

namespace med // "midi et demi"
{

enum class CtrlType { Note, CC };

/** Un contrôle MIDI identifiable (un pad, un potard, un bouton...).
    On ignore volontairement le canal pour rester souple. */
struct Ctrl
{
    CtrlType type   = CtrlType::Note;
    int      number = 0;

    bool operator== (const Ctrl& other) const noexcept
    {
        return type == other.type && number == other.number;
    }
};

/**
    Table de mapping PURE (sans JUCE) : associe des "slots" d'action (indices
    fixes connus de l'app) à des contrôles MIDI. Règles : un contrôle ne pilote
    qu'une seule action, et une action n'a qu'un seul contrôle. Testable seule.
*/
class MidiMap
{
public:
    static constexpr int numSlots = 29;

    /** Associe le contrôle c au slot. Retire c d'un éventuel autre slot. */
    void bind (int slot, Ctrl c) noexcept
    {
        if (slot < 0 || slot >= numSlots)
            return;

        for (int s = 0; s < numSlots; ++s)
            if (used[s] && bindings[s] == c)
                used[s] = false;

        bindings[slot] = c;
        used[slot]     = true;
    }

    void clearSlot (int slot) noexcept
    {
        if (slot >= 0 && slot < numSlots)
            used[slot] = false;
    }

    /** Slot piloté par ce contrôle, ou -1. */
    int findSlot (Ctrl c) const noexcept
    {
        for (int s = 0; s < numSlots; ++s)
            if (used[s] && bindings[s] == c)
                return s;
        return -1;
    }

    bool slotHasBinding (int slot) const noexcept
    {
        return slot >= 0 && slot < numSlots && used[slot];
    }

    Ctrl slotBinding (int slot) const noexcept
    {
        return bindings[slot];
    }

private:
    std::array<Ctrl, numSlots> bindings {};
    std::array<bool, numSlots> used {};
};

} // namespace med

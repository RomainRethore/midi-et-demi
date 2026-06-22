// Tests de la table de mapping (C++ pur, sans JUCE).
//   g++ -std=c++17 -I src tests/midimap_tests.cpp -o midimap_tests && ./midimap_tests

#include "domain/MidiMap.h"

#include <iostream>
#include <string>

namespace
{
    int testsRun = 0, testsFailed = 0;
    void check (bool c, const std::string& l)
    {
        ++testsRun;
        if (! c) { ++testsFailed; std::cout << "  [ECHEC] " << l << "\n"; }
    }
}

int main()
{
    using namespace med;

    const Ctrl cc21 { CtrlType::CC, 21 };
    const Ctrl note36 { CtrlType::Note, 36 };

    // --- binding simple + recherche --------------------------------------
    {
        MidiMap m;
        m.bind (0, cc21);
        check (m.findSlot (cc21) == 0, "CC21 -> slot 0");
        check (m.findSlot (note36) == -1, "Note36 non mappe");
        check (m.slotHasBinding (0), "slot 0 a un binding");
    }

    // --- un contrôle ne pilote qu'une action -----------------------------
    {
        MidiMap m;
        m.bind (0, cc21);
        m.bind (1, cc21);                 // CC21 doit quitter le slot 0
        check (m.findSlot (cc21) == 1, "CC21 reassigne au slot 1");
        check (! m.slotHasBinding (0), "slot 0 libere");
    }

    // --- une action n'a qu'un contrôle (réapprentissage) -----------------
    {
        MidiMap m;
        m.bind (3, note36);
        m.bind (3, cc21);                 // remplace le contrôle du slot 3
        check (m.slotBinding (3) == cc21, "slot 3 -> CC21 apres reapprentissage");
        check (m.findSlot (note36) == -1, "Note36 n'est plus mappe");
    }

    // --- clear ------------------------------------------------------------
    {
        MidiMap m;
        m.bind (5, cc21);
        m.clearSlot (5);
        check (! m.slotHasBinding (5), "slot 5 efface");
        check (m.findSlot (cc21) == -1, "CC21 plus mappe apres clear");
    }

    std::cout << "\n" << (testsRun - testsFailed) << "/" << testsRun << " tests OK\n";
    if (testsFailed > 0) { std::cout << testsFailed << " test(s) en echec.\n"; return 1; }
    std::cout << "Tous les tests passent.\n";
    return 0;
}

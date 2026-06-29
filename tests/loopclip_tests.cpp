// Tests de la boucle MIDI par note (C++ pur, sans JUCE).
//   g++ -std=c++17 -I src tests/loopclip_tests.cpp -o loopclip_tests && ./loopclip_tests

#include "domain/LoopClip.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    int testsRun = 0, testsFailed = 0;
    void check (bool c, const std::string& l)
    {
        ++testsRun;
        if (! c) { ++testsFailed; std::cout << "  [ECHEC] " << l << "\n"; }
    }
    bool nearly (double a, double b, double e = 1e-9) { return std::fabs (a - b) < e; }

    med::Note makeNote (double start, double len, int pitch)
    {
        return { start, len, 1, (uint8_t) pitch, 100 };
    }

    struct Hit { int pitch; double offset; bool on; };

    // Fenêtre avec bouclage, comme le moteur.
    std::vector<Hit> window (const med::LoopClip& clip, double from, double delta)
    {
        std::vector<Hit> hits;
        const double L = clip.getLengthBeats();
        const double to = from + delta;
        auto on  = [&] (const med::Note& n, double off) { hits.push_back ({ n.pitch, off, true }); };
        auto off = [&] (const med::Note& n, double o)   { hits.push_back ({ n.pitch, o, false }); };

        if (to <= L)
        {
            clip.emitWindow (from, to, on, off);
        }
        else
        {
            clip.emitWindow (from, L, on, off);
            const double rem = to - L, base = L - from;
            clip.emitWindow (0.0, rem,
                             [&] (const med::Note& n, double o) { on (n, base + o); },
                             [&] (const med::Note& n, double o) { off (n, base + o); });
        }
        return hits;
    }
}

int main()
{
    using med::LoopClip;

    // --- ajout / taille ---------------------------------------------------
    {
        LoopClip clip; clip.setLengthBeats (4.0);
        clip.addNote (makeNote (0.0, 1.0, 60));
        clip.addNote (makeNote (1.0, 1.0, 62));
        check (clip.size() == 2, "2 notes ajoutees");
    }

    // --- undo / redo par note --------------------------------------------
    {
        LoopClip clip; clip.setLengthBeats (4.0);
        clip.addNote (makeNote (0.0, 1.0, 60));
        clip.addNote (makeNote (1.0, 1.0, 62));
        clip.addNote (makeNote (2.0, 1.0, 64));

        clip.truncate (clip.size() - 1);
        check (clip.size() == 2, "undo : retire la derniere note");
        clip.truncate (clip.size() - 1);
        check (clip.size() == 1, "undo : encore une note");

        clip.restore (clip.size() + 1);
        check (clip.size() == 2, "redo : remet une note");
        clip.restore (clip.size() + 1);
        check (clip.size() == 3, "redo : remet la suivante");

        // nouvelle note apres undo : la redo est abandonnee
        clip.truncate (1);
        clip.addNote (makeNote (0.5, 0.5, 70));
        check (clip.size() == 2, "nouvelle note apres undo -> taille 2");
        clip.restore (3);
        check (clip.size() == 2, "redo impossible apres branche");
    }

    // --- fenêtre on/off ---------------------------------------------------
    {
        LoopClip clip; clip.setLengthBeats (4.0);
        clip.addNote (makeNote (1.0, 1.0, 60)); // on @1, off @2

        auto h = window (clip, 0.0, 1.5); // [0,1.5) -> note-on @1
        check (h.size() == 1 && h[0].on && h[0].pitch == 60 && nearly (h[0].offset, 1.0),
               "note-on a l'offset 1");

        auto h2 = window (clip, 1.5, 1.0); // [1.5,2.5) -> note-off @2
        check (h2.size() == 1 && ! h2[0].on && nearly (h2[0].offset, 0.5),
               "note-off a l'offset 0.5");
    }

    // --- note-off qui boucle ----------------------------------------------
    {
        LoopClip clip; clip.setLengthBeats (4.0);
        clip.addNote (makeNote (3.5, 1.0, 67)); // on @3.5, off @ (4.5 mod 4)=0.5

        auto h = window (clip, 3.0, 2.0); // [3.0,5.0) traverse la boucle
        // attendu : on @3.5 (offset .5) et off @0.5 (offset = (4-3)+0.5 = 1.5)
        bool foundOn = false, foundOff = false;
        for (auto& x : h)
        {
            if (x.on  && nearly (x.offset, 0.5)) foundOn = true;
            if (! x.on && nearly (x.offset, 1.5)) foundOff = true;
        }
        check (foundOn, "note-on @3.5 dans la fenetre bouclee");
        check (foundOff, "note-off bouclé @0.5 dans la fenetre");
    }

    std::cout << "\n" << (testsRun - testsFailed) << "/" << testsRun << " tests OK\n";
    if (testsFailed > 0) { std::cout << testsFailed << " test(s) en echec.\n"; return 1; }
    std::cout << "Tous les tests passent.\n";
    return 0;
}

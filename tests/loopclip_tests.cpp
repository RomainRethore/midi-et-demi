// Tests unitaires de la boucle MIDI (C++ pur, sans JUCE).
//   g++ -std=c++17 -I src tests/loopclip_tests.cpp -o loopclip_tests && ./loopclip_tests

#include "domain/LoopClip.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    int testsRun = 0, testsFailed = 0;

    void check (bool cond, const std::string& label)
    {
        ++testsRun;
        if (! cond) { ++testsFailed; std::cout << "  [ECHEC] " << label << "\n"; }
    }

    // Ajoute une note (on) à un beat donné, vélocité 100, note 60.
    void addNote (med::LoopClip& clip, double beat, int note)
    {
        const uint8_t data[3] = { 0x90, (uint8_t) note, 100 };
        clip.addEvent (beat, data, 3);
    }

    // Reproduit le fenêtrage avec bouclage tel que fait par le moteur.
    struct Hit { int note; double offset; };

    std::vector<Hit> windowWithWrap (const med::LoopClip& clip, double from, double delta)
    {
        std::vector<Hit> hits;
        const double L  = clip.getLengthBeats();
        const double to = from + delta;

        auto emit = [&] (const med::ClipEvent& e, double off)
        {
            hits.push_back ({ (int) e.bytes[1], off });
        };

        if (to <= L)
        {
            clip.forEachInWindow (from, to, emit);
        }
        else
        {
            clip.forEachInWindow (from, L, emit);
            const double rem = to - L, base = L - from;
            clip.forEachInWindow (0.0, rem, [&] (const med::ClipEvent& e, double off)
            {
                emit (e, base + off);
            });
        }
        return hits;
    }

    bool nearly (double a, double b, double eps = 1e-9) { return std::fabs (a - b) < eps; }
}

int main()
{
    using med::LoopClip;

    // --- fenêtre simple, sans bouclage ------------------------------------
    {
        LoopClip clip;
        clip.setLengthBeats (4.0);
        addNote (clip, 0.0, 60);
        addNote (clip, 1.0, 62);
        addNote (clip, 2.0, 64);
        addNote (clip, 3.0, 65);

        auto hits = windowWithWrap (clip, 0.0, 2.0); // [0, 2)
        check (hits.size() == 2, "fenetre [0,2) contient 2 evenements");
        check (hits.size() == 2 && hits[0].note == 60 && nearly (hits[0].offset, 0.0),
               "1er evenement = note 60 a l'offset 0");
        check (hits.size() == 2 && hits[1].note == 62 && nearly (hits[1].offset, 1.0),
               "2e evenement = note 62 a l'offset 1");
    }

    // --- borne haute exclue -----------------------------------------------
    {
        LoopClip clip;
        clip.setLengthBeats (4.0);
        addNote (clip, 2.0, 64);
        auto hits = windowWithWrap (clip, 0.0, 2.0); // 2.0 exclu
        check (hits.empty(), "borne haute exclue (pas d'evenement a beat=2 dans [0,2))");
    }

    // --- fenêtre qui boucle ------------------------------------------------
    {
        LoopClip clip;
        clip.setLengthBeats (4.0);
        addNote (clip, 3.5, 70); // juste avant la fin
        addNote (clip, 0.2, 50); // juste après le début

        // fenêtre [3.0, 3.0+1.5) = [3.0, 4.5) -> boucle a 4.0
        auto hits = windowWithWrap (clip, 3.0, 1.5);
        check (hits.size() == 2, "fenetre a cheval sur la boucle : 2 evenements");
        check (hits.size() == 2 && hits[0].note == 70 && nearly (hits[0].offset, 0.5),
               "note 70 a l'offset 0.5 (avant la boucle)");
        check (hits.size() == 2 && hits[1].note == 50 && nearly (hits[1].offset, 1.2),
               "note 50 a l'offset 1.2 (apres la boucle)");
    }

    std::cout << "\n" << (testsRun - testsFailed) << "/" << testsRun << " tests OK\n";
    if (testsFailed > 0) { std::cout << testsFailed << " test(s) en echec.\n"; return 1; }
    std::cout << "Tous les tests passent.\n";
    return 0;
}

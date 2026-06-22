// Tests unitaires de la couche domaine (C++ pur, sans JUCE).
// Compilation / exécution :
//   g++ -std=c++17 -I src tests/transport_tests.cpp -o transport_tests && ./transport_tests

#include "domain/Transport.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
    int testsRun    = 0;
    int testsFailed = 0;

    void check (bool condition, const std::string& label)
    {
        ++testsRun;
        if (! condition)
        {
            ++testsFailed;
            std::cout << "  [ECHEC] " << label << "\n";
        }
    }

    bool nearlyEqual (double a, double b, double eps = 1e-6)
    {
        return std::fabs (a - b) < eps;
    }
}

int main()
{
    using med::Transport;

    // --- samplesPerBeat ---------------------------------------------------
    {
        Transport t;
        t.prepare (44100.0);
        t.setTempo (120.0);                       // 120 noires/min => 0,5 s/temps
        check (nearlyEqual (t.samplesPerBeat(), 22050.0), "120 BPM @44100 = 22050 samples/temps");

        t.setTempo (60.0);                        // 60 BPM => 1 s/temps
        check (nearlyEqual (t.samplesPerBeat(), 44100.0), "60 BPM @44100 = 44100 samples/temps");
    }

    // --- signature rythmique ----------------------------------------------
    {
        Transport t;
        t.prepare (48000.0);
        t.setTempo (120.0);
        t.setTimeSignature (4, 8);                // temps = croche => moitié d'une noire
        check (nearlyEqual (t.samplesPerBeat(), 48000.0 * 0.5 * 0.5),
               "120 BPM en x/8 => moitie d'une noire");
    }

    // --- avance & position en temps ---------------------------------------
    {
        Transport t;
        t.prepare (44100.0);
        t.setTempo (120.0);

        t.advance (10000);                        // arrêté => ne bouge pas
        check (nearlyEqual (t.getPositionSamples(), 0.0), "avance ignoree a l'arret");

        t.start();
        t.advance (22050);                         // exactement 1 temps
        check (nearlyEqual (t.getPositionInBeats(), 1.0), "1 temps apres avance d'un temps");

        t.advance (22050);                         // 2e temps
        check (nearlyEqual (t.getPositionInBeats(), 2.0), "2 temps apres deux avances");
    }

    // --- start / stop / rewind --------------------------------------------
    {
        Transport t;
        t.prepare (44100.0);
        t.start();
        check (t.isPlaying(), "isPlaying vrai apres start");

        t.advance (1000);
        t.stop();
        check (! t.isPlaying(), "isPlaying faux apres stop");

        double posAvant = t.getPositionSamples();
        t.advance (5000);
        check (nearlyEqual (t.getPositionSamples(), posAvant), "position figee a l'arret");

        t.rewind();
        check (nearlyEqual (t.getPositionSamples(), 0.0), "rewind remet a zero");
    }

    // --- bilan ------------------------------------------------------------
    std::cout << "\n" << (testsRun - testsFailed) << "/" << testsRun << " tests OK\n";
    if (testsFailed > 0)
    {
        std::cout << testsFailed << " test(s) en echec.\n";
        return 1;
    }

    std::cout << "Tous les tests passent.\n";
    return 0;
}

#pragma once

#include <cmath>

namespace med
{

/**
    Générateur de "clic" de métronome (C++ pur). Une courte sinusoïde qui
    décroît rapidement ; accent plus aigu sur le premier temps de la mesure.

    Conçu pour le fil audio : aucune allocation, tout est précalculé.
    Le déclenchement se fait à l'échantillon près via trigger(), puis on lit le
    signal échantillon par échantillon avec nextSample().
*/
class Metronome
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        envStep    = 1.0 / (0.030 * sampleRate); // décroissance sur ~30 ms
        reset();
    }

    void reset() noexcept
    {
        env   = 0.0;
        phase = 0.0;
    }

    void trigger (bool downbeat) noexcept
    {
        env        = 1.0;
        phase      = 0.0;
        auto freq  = downbeat ? 1760.0 : 1100.0; // accent aigu sur le 1er temps
        angleDelta = freq * twoPi / sampleRate;
    }

    /** Renvoie l'échantillon de clic courant (mono) et avance l'enveloppe. */
    float nextSample() noexcept
    {
        if (env <= 0.0)
            return 0.0f;

        auto sample = (float) (std::sin (phase) * env) * gain;

        phase += angleDelta;
        env   -= envStep;
        if (env < 0.0)
            env = 0.0;

        return sample;
    }

private:
    static constexpr double twoPi = 6.283185307179586;

    double sampleRate = 44100.0;
    double phase      = 0.0;
    double angleDelta = 0.0;
    double env        = 0.0;
    double envStep    = 0.0;
    float  gain       = 0.25f;
};

} // namespace med

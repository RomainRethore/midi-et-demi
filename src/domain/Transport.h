#pragma once

namespace med // "midi et demi"
{

/**
    Horloge musicale PURE (aucune dépendance à JUCE).

    Elle convertit le temps en échantillons / battements / mesures, gère
    lecture / arrêt, et avance la position. Étant pure, elle se teste isolément
    avec un simple compilateur C++ (cf. tests/transport_tests.cpp).

    Convention : le BPM est exprimé en noires/minute. La durée d'un "temps"
    dépend du dénominateur de la signature (4 = noire, 8 = croche, ...).
*/
class Transport
{
public:
    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate; }

    void   setTempo (double newBpm) noexcept { if (newBpm > 0.0) bpm = newBpm; }
    double getTempo() const noexcept          { return bpm; }

    void setTimeSignature (int numeratorToUse, int denominatorToUse) noexcept
    {
        if (numeratorToUse   > 0) numerator   = numeratorToUse;
        if (denominatorToUse > 0) denominator = denominatorToUse;
    }
    int getNumerator()   const noexcept { return numerator; }
    int getDenominator() const noexcept { return denominator; }

    void start() noexcept { playing = true; }
    void stop()  noexcept { playing = false; }
    bool isPlaying() const noexcept { return playing; }

    void setPositionSamples (double s) noexcept { positionSamples = s; }
    void rewind() noexcept { positionSamples = 0.0; }

    double getSampleRate()      const noexcept { return sampleRate; }
    double getPositionSamples() const noexcept { return positionSamples; }

    /** Nombre d'échantillons par temps (battement). */
    double samplesPerBeat() const noexcept
    {
        return sampleRate * (60.0 / bpm) * (4.0 / (double) denominator);
    }

    /** Position courante exprimée en temps depuis le début. */
    double getPositionInBeats() const noexcept
    {
        auto spb = samplesPerBeat();
        return spb > 0.0 ? positionSamples / spb : 0.0;
    }

    /** Avance la position de numSamples — uniquement si en lecture. */
    void advance (int numSamples) noexcept
    {
        if (playing)
            positionSamples += (double) numSamples;
    }

private:
    double sampleRate      = 44100.0;
    double bpm             = 120.0;
    int    numerator       = 4;
    int    denominator     = 4;
    double positionSamples = 0.0;
    bool   playing         = false;
};

} // namespace med

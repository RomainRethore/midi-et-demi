#pragma once

namespace med // "midi et demi"
{

/**
    Horloge musicale PURE (sans JUCE).

    La position est stockée **en temps (battements)** et avancée de façon
    incrémentale (positionBeats += numSamples / samplesParTemps). Ainsi un
    changement de tempo n'affecte que la suite — la position ne « saute » jamais
    (sinon des note-off seraient sautés, laissant des notes tenues).
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

    void rewind() noexcept { positionBeats = 0.0; }

    double getSampleRate() const noexcept { return sampleRate; }

    /** Nombre d'échantillons par temps (battement). */
    double samplesPerBeat() const noexcept
    {
        return sampleRate * (60.0 / bpm) * (4.0 / (double) denominator);
    }

    /** Position courante en temps depuis le début (jamais de saut au changement
        de tempo). */
    double getPositionInBeats() const noexcept { return positionBeats; }

    /** Position en échantillons, dérivée du temps courant. */
    double getPositionSamples() const noexcept { return positionBeats * samplesPerBeat(); }

    /** Avance la position de numSamples — uniquement si en lecture. */
    void advance (int numSamples) noexcept
    {
        if (! playing)
            return;

        const auto spb = samplesPerBeat();
        if (spb > 0.0)
            positionBeats += (double) numSamples / spb;
    }

private:
    double sampleRate    = 44100.0;
    double bpm           = 120.0;
    int    numerator     = 4;
    int    denominator   = 4;
    double positionBeats = 0.0;
    bool   playing       = false;
};

} // namespace med

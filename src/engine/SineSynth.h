#pragma once

#include <JuceHeader.h>

/**
    Synthé "jouet" de l'incrément 1a : une simple onde sinusoïdale avec
    enveloppe ADSR, juste pour prouver que la chaîne MIDI -> son fonctionne.
    Il sera remplacé par un vrai plugin instrument à l'incrément 1b.
*/

/** Son accepté par toutes les notes / tous les canaux. */
struct SineSound : public juce::SynthesiserSound
{
    bool appliesToNote (int /*midiNoteNumber*/) override    { return true; }
    bool appliesToChannel (int /*midiChannel*/) override    { return true; }
};

/** Une voix = une note jouée (le synthé en empile plusieurs pour la polyphonie). */
class SineVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SineSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        currentAngle = 0.0;
        level        = velocity * 0.15; // on garde de la marge pour éviter de saturer
        auto freqHz  = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        angleDelta   = freqHz * juce::MathConstants<double>::twoPi / getSampleRate();

        adsr.setSampleRate (getSampleRate());
        adsr.setParameters ({ 0.01f, 0.10f, 0.80f, 0.20f }); // Attack, Decay, Sustain, Release
        adsr.noteOn();
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff(); // on laisse le relâchement se terminer dans renderNextBlock
        }
        else
        {
            adsr.reset();
            clearCurrentNote();
            angleDelta = 0.0;
        }
    }

    void pitchWheelMoved (int /*newValue*/) override {}
    void controllerMoved (int /*controllerNumber*/, int /*newValue*/) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (angleDelta == 0.0)
            return;

        while (--numSamples >= 0)
        {
            auto env    = adsr.getNextSample();
            auto sample = (float) (std::sin (currentAngle) * level) * env;

            for (int ch = output.getNumChannels(); --ch >= 0;)
                output.addSample (ch, startSample, sample);

            currentAngle += angleDelta;
            ++startSample;

            if (! adsr.isActive())
            {
                clearCurrentNote();
                angleDelta = 0.0;
                break;
            }
        }
    }

private:
    double      currentAngle = 0.0;
    double      angleDelta   = 0.0;
    double      level        = 0.0;
    juce::ADSR  adsr;
};

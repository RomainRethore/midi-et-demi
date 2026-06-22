#pragma once

#include <JuceHeader.h>

/**
    Couche ENGINE : héberge les plugins instruments (VST3 / AU).
    Pour l'instant (1b) : charge un plugin à partir d'un fichier choisi par
    l'utilisateur. Le scan automatique + dropdown viendra dans un incrément suivant.
*/
class PluginHost
{
public:
    PluginHost();

    /** Crée une instance de plugin à partir d'un fichier (.vst3 / .component).
        Retourne nullptr et renseigne 'errorMessage' en cas d'échec. */
    std::unique_ptr<juce::AudioPluginInstance> createFromFile (const juce::File& file,
                                                               double sampleRate,
                                                               int blockSize,
                                                               juce::String& errorMessage);

private:
    juce::AudioPluginFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginHost)
};

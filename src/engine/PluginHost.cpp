#include "engine/PluginHost.h"

PluginHost::PluginHost()
{
    // Enregistre les formats disponibles (VST3 + AudioUnit sur Mac),
    // selon les defines JUCE_PLUGINHOST_* activés dans le CMakeLists.
    formatManager.addDefaultFormats();
}

std::unique_ptr<juce::AudioPluginInstance>
PluginHost::createFromFile (const juce::File& file, double sampleRate, int blockSize,
                            juce::String& errorMessage)
{
    juce::OwnedArray<juce::PluginDescription> descriptions;

    // On demande à chaque format s'il reconnaît ce fichier ; on s'arrête au premier.
    for (auto* format : formatManager.getFormats())
    {
        juce::KnownPluginList list;
        list.scanAndAddFile (file.getFullPathName(), true, descriptions, *format);

        if (! descriptions.isEmpty())
            break;
    }

    if (descriptions.isEmpty())
    {
        errorMessage = "Aucun plugin reconnu dans : " + file.getFileName();
        return nullptr;
    }

    // createPluginInstance renseigne errorMessage si l'instanciation échoue.
    return formatManager.createPluginInstance (*descriptions.getFirst(),
                                               sampleRate, blockSize, errorMessage);
}

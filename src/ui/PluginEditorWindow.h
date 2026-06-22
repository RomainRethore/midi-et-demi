#pragma once

#include <JuceHeader.h>

/**
    Fenêtre qui affiche l'interface graphique native d'un plugin.
    L'éditeur appartient au plugin (setContentNonOwned) : fermer la fenêtre ne
    le détruit pas, c'est la destruction du plugin qui s'en charge.
*/
class PluginEditorWindow : public juce::DocumentWindow
{
public:
    explicit PluginEditorWindow (juce::AudioPluginInstance& plugin)
        : DocumentWindow (plugin.getName(),
                          juce::Colours::darkgrey,
                          juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);

        if (auto* editor = plugin.createEditorIfNeeded())
            setContentNonOwned (editor, true);
        else
            setContentOwned (new juce::Label ({}, "Ce plugin n'a pas d'interface graphique."),
                             true);

        centreWithSize (juce::jmax (400, getWidth()),
                        juce::jmax (200, getHeight()));
        setVisible (true);
    }

    /** Appelé quand l'utilisateur clique la croix de fermeture. */
    std::function<void()> onCloseButton;

    void closeButtonPressed() override
    {
        if (onCloseButton)
            onCloseButton();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditorWindow)
};

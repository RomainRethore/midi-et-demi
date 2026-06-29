#pragma once

#include <JuceHeader.h>
#include "engine/AudioEngine.h"

namespace medui
{
    inline bool isAudioFile (const juce::String& path)
    {
        auto p = path.toLowerCase();
        return p.endsWith (".wav") || p.endsWith (".aif") || p.endsWith (".aiff")
            || p.endsWith (".flac") || p.endsWith (".ogg");
    }
}

/**
    Navigateur de samples : parcourt un dossier (sous-dossiers + fichiers audio),
    écoute un fichier d'un clic (pré-écoute), et permet de **glisser** un fichier
    vers un pad (le ListBox démarre un drag via getDragSourceDescription, capté
    par le DragAndDropContainer parent).
*/
class SampleBrowser : public juce::Component,
                      public juce::ListBoxModel
{
public:
    explicit SampleBrowser (AudioEngine& e) : engine (e)
    {
        addAndMakeVisible (pathLabel);
        pathLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff20232c));
        pathLabel.setMinimumHorizontalScale (0.5f);

        addAndMakeVisible (upButton);
        upButton.setButtonText ("^");
        upButton.onClick = [this] { setDirectory (currentDir.getParentDirectory()); };

        addAndMakeVisible (folderButton);
        folderButton.setButtonText ("Dossier...");
        folderButton.onClick = [this] { chooseFolder(); };

        addAndMakeVisible (stopButton);
        stopButton.setButtonText ("Stop");
        stopButton.onClick = [this] { engine.stopPreview(); };

        addAndMakeVisible (listBox);
        listBox.setModel (this);
        listBox.setRowHeight (22);
        listBox.setMultipleSelectionEnabled (true); // Cmd/Maj+clic pour multi-sélection

        addAndMakeVisible (assignButton);
        assignButton.setButtonText ("Assigner aux pads (1->16)");
        assignButton.onClick = [this] { assignSelectionToPads(); };

        setDirectory (juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    }

    void setDirectory (const juce::File& dir)
    {
        if (! dir.isDirectory())
            return;

        currentDir = dir;
        entries.clear();

        for (auto& d : dir.findChildFiles (juce::File::findDirectories, false))
            entries.add ({ d, true });
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false))
            if (medui::isAudioFile (f.getFileName()))
                entries.add ({ f, false });

        pathLabel.setText (dir.getFullPathName(), juce::dontSendNotification);
        listBox.deselectAllRows();
        listBox.updateContent();
        listBox.repaint();
    }

    // --- ListBoxModel ---
    int getNumRows() override { return entries.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= entries.size())
            return;
        if (selected)
        {
            g.setColour (juce::Colour (0xff3a3f55));
            g.fillRect (0, 0, w, h);
        }
        const auto& e = entries[row];
        g.setColour (e.isDir ? juce::Colours::lightblue : juce::Colours::white);
        g.setFont (13.0f);
        g.drawText ((e.isDir ? juce::String ("[ ] ") : juce::String())
                        + e.file.getFileName(),
                    6, 0, w - 8, h, juce::Justification::centredLeft);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (row < 0 || row >= entries.size())
            return;
        if (entries[row].isDir)
            setDirectory (entries[row].file);
        else
            engine.previewSample (entries[row].file); // écoute au clic
    }

    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override
    {
        if (rows.size() > 0)
        {
            const int r = rows[0];
            if (r >= 0 && r < entries.size() && ! entries[r].isDir)
                return entries[r].file.getFullPathName(); // active le glisser
        }
        return {};
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (4);
        auto top = a.removeFromTop (26);
        upButton    .setBounds (top.removeFromLeft (30).reduced (1));
        folderButton.setBounds (top.removeFromLeft (90).reduced (1));
        stopButton  .setBounds (top.removeFromRight (60).reduced (1));
        pathLabel   .setBounds (top.reduced (2));
        a.removeFromTop (4);
        assignButton.setBounds (a.removeFromBottom (28).reduced (0, 1));
        a.removeFromBottom (4);
        listBox.setBounds (a);
    }

private:
    /** Assigne les fichiers sélectionnés (dans l'ordre) aux pads 1..16. */
    void assignSelectionToPads()
    {
        const auto rows = listBox.getSelectedRows();
        int pad = 0;
        for (int i = 0; i < rows.size() && pad < 16; ++i)
        {
            const int r = rows[i];
            if (r >= 0 && r < entries.size() && ! entries[r].isDir)
                engine.loadSampleToActiveTrack (pad++, entries[r].file);
        }
    }

    void chooseFolder()
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Dossier de samples", currentDir);
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                auto d = fc.getResult();
                if (d != juce::File{})
                    setDirectory (d);
            });
    }

    struct Entry { juce::File file; bool isDir; };

    AudioEngine&        engine;
    juce::File          currentDir;
    juce::Array<Entry>  entries;
    juce::ListBox       listBox;
    juce::Label         pathLabel;
    juce::TextButton    upButton, folderButton, stopButton, assignButton;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleBrowser)
};

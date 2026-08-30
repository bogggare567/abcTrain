#pragma once

#include <atomic>
#include <memory>

#include "PluginProcessor.h"
#include <functional>

// "Choose training sounds" overlay: lets a player pick which folder of
// their own reference audio (see ReferenceAudioLibrary) EarTrainer's
// games should use instead of synthesized pink noise, or switch back to
// pink noise. Categories are just whatever subfolders currently exist
// under the configured root folder - this component draws no audio
// content itself and ships none; see decisions/015. Meant to be added as
// a full-size child of EarTrainerEditor and toggled visible via a
// "Training Sounds" button, same show/hide shape as shared/LessonController.
//
// Two panes. The left rail is what to train on - pink noise, then every
// category the library knows about; the right pane is the actual files in
// whichever one is selected, and clicking one pins training to it.
//
// The previous version listed categories only, picked a clip at random
// inside them, and never showed a filename or a path. It was possible to
// import an album and have no way to find out what had happened to it.
// Rotation is still the default and still the better one - twenty drum
// loops should be twenty drum loops - but "let me hear *that* one" is a
// real request and now has an answer.
//
// Categories used to unlock with the player's level. That made sense for
// shipped content and none at all for a folder of the player's own music:
// files you imported yourself should never be locked behind anything.
class TrainingSoundsComponent : public juce::Component
{
public:
    explicit TrainingSoundsComponent (EarTrainerProcessor& processorToControl);

    std::function<void()> onClosed;

    // Rescans the root folder and rebuilds the category list/lock state -
    // call every time this overlay is shown, in case the folder or the
    // player's level changed since it was last opened.
    void refresh();

    void paint (juce::Graphics&) override;
    // Out of line, so it sees a complete ImportJob - a unique_ptr to an
    // incomplete type cannot be destroyed by a compiler-generated
    // destructor.
    ~TrainingSoundsComponent() override;

    void resized() override;

    // Localised strings, pushed in by the editor - this component keeps no
    // LocalisationManager of its own, like every other view here.
    //
    // A struct rather than a parameter list. It was twelve positional
    // arguments of one type, which is a call site nobody can read and
    // where swapping two of them compiles cleanly - and it is why the
    // three strings this panel still drew in English were never added.
    struct Strings
    {
        juce::String title, sourceSection, trainOnSection, chooseFolder, openFolder;
        juce::String pinkNoise, close, empty, pickCategory, clipsHeading;
        juce::String importAndSort, importing, importedClips, importedNothing, importHint;
        juce::String trainingOnPinkNoise, trainingOnFile, shuffling;
        juce::String builtInPercussive, builtInSustained;
    };

    void setStrings (Strings);

private:
    juce::Rectangle<int> cardBounds() const;

    // One row per option. A grid of equal-width buttons cannot carry a
    // count and a tick, which are the two things that tell you what you
    // are choosing between.
    static constexpr int rowHeight = 32;

    Strings text;
    juce::String previousHint;

    // The two categories this app supplies itself, which are the only ones
    // whose names it is entitled to translate: everything else in the list
    // is a folder somebody made, and renaming a person's folder on screen
    // is how you make them unable to find it on disk.
    juce::String displayNameForCategory (const juce::String& rawName) const;

    void importAndSort();
    juce::TextButton importButton;

    // The import runs on its own thread: decoding and analysing a handful
    // of full-length tracks takes real seconds, and a window that freezes
    // for them reads as a crash. The thread only touches the library and
    // two atomics; everything that changes the UI comes back through
    // callAsync on the message thread.
    class ImportJob;
    std::unique_ptr<ImportJob> importJob;

    std::atomic<float> importProgress { 0.0f };
    juce::String importProgressFile;
    bool importRunning = false;

    void startImport (const juce::Array<juce::File>& files);
    void finishImport (int clipsWritten);
    void paintImportProgress (juce::Graphics&, juce::Rectangle<int>);

    void selectCategory (int categoryIndex);
    void updateStatusLabel();

    EarTrainerProcessor& processor;
    juce::Random random;

    juce::Label titleLabel;

    // Lets the player point ReferenceAudioLibrary at their own folder of
    // audio files (e.g. their personal music library) - the legitimate
    // way to train on real material without this project ever fetching,
    // bundling, or vetting any of it itself (see decisions/015 and 018).
    juce::TextButton chooseFolderButton;
    juce::Label rootFolderLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Label statusLabel;
    juce::TextButton closeButton;
    juce::TextButton revealButton;

    // -1 is pink noise; 0.. index into the library's categories.
    int selectedCategory = -1;
    int hoveredCategoryRow = -2;
    int hoveredFileRow = -1;
    float fileScroll = 0.0f;
    float maxFileScroll = 0.0f;

    juce::Rectangle<int> railBounds() const;
    juce::Rectangle<int> filePaneBounds() const;
    juce::Rectangle<int> categoryRowBounds (int index) const;   // -1 = pink noise
    juce::Rectangle<int> fileRowBounds (int index) const;

    const juce::Array<juce::File>* filesForSelection() const;
    void selectPinkNoise();
    void pinFile (int fileIndex);
    void paintRail (juce::Graphics&);
    void paintFilePane (juce::Graphics&);

public:
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrainingSoundsComponent)
};

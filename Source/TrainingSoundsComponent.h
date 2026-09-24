#pragma once

#include <atomic>
#include <memory>

#include "PluginProcessor.h"
#include <functional>
#include <map>

// "Choose training sounds" page: each exercise's own synthesized sound
// (the default since ADR 040), pink noise, or a folder of audio - the
// player's own, or a pack with authors and tags (docs/design/sound-library.md). Categories are just whatever subfolders currently exist
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
// September 2026: laid out like a DAW's sample browser. Every clip shows
// its waveform and has a play button; the chosen one is drawn large with
// a playhead; "All clips / This clip only" says in two words what the old
// tick-and-footer combination took a sentence to explain.
class TrainingSoundsComponent : public juce::Component,
                                private juce::Timer
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
        juce::String exerciseSound, trainingOnExerciseSound, credits;
        juce::String clipsCaption, allClips, thisClip;   // "{{n}} clips · click to hear..."

        // 2026-09-24: instruments, fragments, deleting.
        juce::StringArray instrumentNames;     // InstrumentLabel order
        juce::String mySounds;                 // rail heading over the player's own folders
        juce::String fragmentFromTrack;        // rail button
        juce::String selectHint;               // under the big waveform
        juce::String trackHint;                // caption in track mode
        juce::String saveFragment, cancelSelection;
        juce::String fragmentOnGrid;           // "{{bars}} bars at {{bpm}} BPM"
        juce::String fragmentOnBeats;          // "{{beats}} beats at {{bpm}} BPM"
        juce::String fragmentFree;             // "{{seconds}} s, no steady tempo"
        juce::String fragmentSaved;            // "Saved: {{what}} -> {{folder}}"
        juce::String fragmentTooShort;
        juce::String deleteClip, deleteConfirm, deleteYes, deleteNo, deleted;

        juce::String languageCode;   // picks a pack's title: "ru" or anything else
    };

    void setStrings (Strings);

    // For tools/EditorSnapshots: show a category's clips with one focused,
    // without changing what the library trains on (that is persisted).
    // For tools/EditorSnapshots: a selection on the big waveform, and a
    // row asking whether to delete.
    void selectForSnapshot (float from, float to) { selectionStart = from; selectionEnd = to; updateModeButtons(); resized(); repaint(); }
    void askDeleteForSnapshot (int row) { pendingDelete = row; repaint(); }
    void openTrackForSnapshot (const juce::File& file) { openTrack (file); }

    void browseForSnapshot (const juce::String& categoryName, int fileToFocus)
    {
        const auto& categories = processor.getGameManager().getReferenceAudioLibrary().getCategories();

        for (int i = 0; i < categories.size(); ++i)
            if (categories.getReference (i).name == categoryName)
                selectedCategory = i;

        focusedFile = fileToFocus;
        updateModeButtons();
        repaint();
    }

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

    // ---- your own fragment (2026-09-24) ----
    // Drag across the big waveform to select; Save cuts it into a loop
    // (ReferenceAudioLibrary::saveFragment). In "track" mode the big
    // waveform is a whole file picked from disk rather than a library clip.
    juce::TextButton fragmentButton, saveSelectionButton, cancelSelectionButton;
    juce::File sourceTrack;
    juce::Array<juce::File> trackFiles;     // { sourceTrack } - what filesForSelection returns then
    float selectionStart = -1.0f, selectionEnd = -1.0f;   // fractions of the focused file
    bool selecting = false;
    int dragStartX = 0;
    void chooseTrackForFragment();
    void openTrack (const juce::File&);
    void clearSelection();
    bool hasSelection() const noexcept { return selectionStart >= 0.0f && selectionEnd > selectionStart; }
    void saveSelection();

    // Playing a stretch of a long file: the buffer covers [offset, offset
    // + span) of it, as fractions, so the playhead lands in the right place.
    float playOffset = 0.0f, playSpan = 1.0f;

    // ---- deleting ----
    int pendingDelete = -1;                 // row asking "delete?"
    juce::Rectangle<int> rowDeleteBounds (int index) const;
    juce::Rectangle<int> confirmYesBounds (int index) const;
    juce::Rectangle<int> confirmNoBounds (int index) const;
    void deleteFile (int index);

    // ---- the rail as a list with headings, scrolled when it is long ----
    struct RailEntry { int category; juce::String heading; };   // category == noRow for a heading
    std::vector<RailEntry> railEntries;
    void rebuildRail();
    juce::Rectangle<int> railListBounds() const;
    float railScroll = 0.0f;

    void chooseFilesToImport();

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

    // Rail rows: the exercise's own sound, pink noise, then 0.. the
    // library's categories.
    static constexpr int exerciseRow = -2;
    static constexpr int pinkNoiseRow = -1;
    static constexpr int noRow = -3;

    int selectedCategory = -1;
    int hoveredCategoryRow = noRow;
    int hoveredFileRow = -1;
    float fileScroll = 0.0f;
    float maxFileScroll = 0.0f;

    juce::Rectangle<int> railBounds() const;
    juce::Rectangle<int> filePaneBounds() const;
    juce::Rectangle<int> categoryRowBounds (int index) const;   // exerciseRow, pinkNoiseRow, 0..
    juce::Rectangle<int> fileRowBounds (int index) const;

    const juce::Array<juce::File>* filesForSelection() const;
    const ReferenceAudioLibrary::Category* selectedCategoryInfo() const;
    void selectPinkNoise();
    void selectExerciseSound();
    void pinFile (int fileIndex);
    void paintRail (juce::Graphics&);
    void paintFilePane (juce::Graphics&);
    void paintPreview (juce::Graphics&, juce::Rectangle<int>);
    void paintWave (juce::Graphics&, juce::Rectangle<float>, const std::vector<float>&,
                    juce::Colour, float playedFraction, juce::Colour playedColour);

    // ---- the browser ----
    void timerCallback() override;

    // Peaks and length per file, read once and kept for the page's life.
    const ClipPreview::Overview& overviewFor (const juce::File&);
    const ClipPreview::Overview& longOverviewFor (const juce::File&);   // a whole track, streamed
    std::map<juce::String, ClipPreview::Overview> overviews;
    juce::AudioFormatManager formats;

    // The clip drawn large: the pinned one, else the one last clicked.
    int focusedFile = -1;
    int playingFile = -1;
    void focusFile (int index);
    void togglePlay (int index, float from = 0.0f);
    void stopPreview();

    juce::Rectangle<int> previewBounds() const;
    juce::Rectangle<int> previewWaveBounds() const;
    juce::Rectangle<int> previewPlayBounds() const;
    juce::Rectangle<int> listBounds() const;
    juce::Rectangle<int> rowPlayBounds (int index) const;

    juce::TextButton allClipsButton, thisClipButton;
    void updateModeButtons();

public:
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void visibilityChanged() override;

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrainingSoundsComponent)
};

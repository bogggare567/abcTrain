#pragma once

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
// Category tiles unlock progressively with ProgressManager's
// maxLevelReached (tile at index i unlocks once maxLevelReached > i) -
// the same "progress you can see and control" idea as the level selector
// (decisions/014), but a simple first-pass rule rather than a hand-
// curated unlock plan, since these categories are whatever the user's
// own folder happens to contain, not fixed shipped content.
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
    void resized() override;

    // Localised strings, pushed in by the editor - this component keeps no
    // LocalisationManager of its own, like every other view here.
    void setStrings (juce::String title, juce::String sourceSection, juce::String trainOnSection,
                     juce::String chooseFolder, juce::String pinkNoise, juce::String close,
                     juce::String emptyText, juce::String importAndSort, juce::String importing,
                     juce::String importedClips, juce::String importedNothing);

private:
    juce::Rectangle<int> cardBounds() const;

    static constexpr int categoryColumns = 2;
    static constexpr int categoryTileHeight = 34;

    juce::String sourceHeading { "Where the sounds come from" };
    juce::String trainOnHeading { "What to train on" };
    juce::String emptyMessage;
    juce::String importingText, importedClipsText, importedNothingText;

    void importAndSort();
    juce::TextButton importButton;

    void selectCategory (int categoryIndex);
    void updateStatusLabel();

    EarTrainerProcessor& processor;
    juce::Random random;

    juce::Label titleLabel;

    // Lets the player point ReferenceAudioLibrary at their own folder of
    // audio files (e.g. their personal music library) - the legitimate
    // way to train on real material without this project ever fetching,
    // bundling, or vetting any of it itself (see decisions/015 and 018).
    juce::TextButton chooseFolderButton { "Choose Folder..." };
    juce::Label rootFolderLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::TextButton pinkNoiseButton { "Pink Noise (default)" };
    juce::OwnedArray<juce::TextButton> categoryButtons;
    juce::Label statusLabel;
    juce::TextButton closeButton { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrainingSoundsComponent)
};

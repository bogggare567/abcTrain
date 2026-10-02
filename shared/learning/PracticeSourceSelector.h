#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/ui/CompactSelector.h"
#include "shared/learning/PracticeAudioSource.h"
#include "shared/audio/ReferenceAudioLibrary.h"
#include <functional>

// The title-row control that decides what a Learner plugin listens to:
// the host, or a clip from the shared reference library.
//
// One component for all three Learner plugins rather than the same forty
// lines pasted into each editor - unlike the Bypass and Updates buttons,
// which are deliberately duplicated because they are two lines of wiring
// each, this carries real behaviour (scan, persist, apply, re-apply at the
// right sample rate) and three copies of it would drift.
//
// "Host" is always first and always the default. A plugin that starts
// playing audio into a session on its own would be a bug, not a feature.
//
// The library is shared with Ear Trainer, so anything imported there is
// already here. Nothing can be imported *from* here on purpose: the import
// screen has a file picker, a progress bar and a slicer behind it, and
// duplicating that into three plugins would be three more places for it to
// go wrong. This offers what exists.
class PracticeSourceSelector : public juce::Component
{
public:
    PracticeSourceSelector (ReferenceAudioLibrary&, PracticeAudioSource&,
                            juce::PropertiesFile&, std::function<double()> sampleRateProvider);

    // Rescans and rebuilds the list, then re-applies the saved choice.
    // Called once on construction; call again if the library may have
    // changed underneath (a new import in another window).
    // The category chosen when nothing was saved yet (the app's Studio:
    // the live vocal; a plugin in a DAW: the host's audio, as before).
    void setDefaultCategory (const juce::String& name) { defaultCategory = name; refresh(); }

    void refresh();

    // The three words it shows, in the plugin's language. Rebuilds the list.
    void setLabels (juce::String caption, juce::String hostAudio, juce::String hostShort)
    {
        captionText = std::move (caption);
        hostText = std::move (hostAudio);
        hostShortText = std::move (hostShort);
        selector.setCaption (captionText);
        refresh();
    }

    int getPreferredWidth() const { return selector.getPreferredWidth(); }

    // The same choices as short words, for a chip row: host first, then
    // one per category. Index = position in this list.
    juce::StringArray getShortLabels() const { return shortLabels; }
    int getChosenIndex() const { return selector.getSelectedId() - 1; }
    void choose (int index)
    {
        selector.setSelectedId (index + 1, juce::dontSendNotification);
        applySelection (true);
    }

    // Called after refresh() rebuilt the list, so a chip row can follow.
    std::function<void()> onListChanged;

    void resized() override;

    static constexpr const char* selectedCategoryKey = "practiceCategory";

private:
    juce::String defaultCategory;
    juce::String captionText { "source" }, hostText { "Host audio" }, hostShortText { "host" };
    void applySelection (bool remember);

    ReferenceAudioLibrary& library;
    PracticeAudioSource& source;
    juce::PropertiesFile& properties;
    std::function<double()> getSampleRate;

    CompactSelector selector;
    juce::StringArray shortLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PracticeSourceSelector)
};

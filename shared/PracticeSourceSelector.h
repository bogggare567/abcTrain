#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CompactSelector.h"
#include "PracticeAudioSource.h"
#include "ReferenceAudioLibrary.h"
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
    void refresh();

    int getPreferredWidth() const { return selector.getPreferredWidth(); }

    void resized() override;

    static constexpr const char* selectedCategoryKey = "practiceCategory";

private:
    void applySelection();

    ReferenceAudioLibrary& library;
    PracticeAudioSource& source;
    juce::PropertiesFile& properties;
    std::function<double()> getSampleRate;

    CompactSelector selector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PracticeSourceSelector)
};

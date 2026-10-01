#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "shared/learning/LearnerEditorBase.h"
#include "shared/ui/KnobRow.h"
#include "shared/ui/SegmentedChoice.h"
#include "shared/analysis/WaveformDisplay.h"
#include "EchogramView.h"
#include "RoomView.h"

// Learner Verb: four reverb types, six knobs, and the same shell as the
// other two Learner plugins (ADR 037).
class LearnerVerbEditor : public LearnerEditorBase
{
public:
    explicit LearnerVerbEditor (LearnerVerbProcessor&);
    ~LearnerVerbEditor() override;

    void applyPresetForSnapshot (int index)
    {
        verbProcessor.applyPreset (index);
        presets.setChosen (index);
        syncType();
        updateEchogram (true);
        updateNotes();
    }

private:
    int analysisContentHeight() const override { return 260; }
    int controlsContentHeight() const override { return knobRowHeight() + KnobRow::noteHeight + controlsFooterHeight() + 3 * rowGap(); }
    int controlsFooterHeight() const override { return 30; }
    void layoutToolbar (juce::Rectangle<int>) override;
    void layoutAnalysis (juce::Rectangle<int>) override;
    void layoutControls (juce::Rectangle<int>) override;
    void themeChanged() override;
    void tick() override;
    void syncType();
    void updateEchogram (bool immediately = false);
    void updateNotes();
    juce::String typeDescription (int type) const;
    int routingCaptionWidth() const;

    LearnerVerbProcessor& verbProcessor;

    RoomView room;
    EchogramView echogram;
    WaveformDisplay waveform;   // fed by the processor; not on screen in this layout

    ChipRow typeChips;
    int shownType = -1;
    KnobRow knobs;
    ChipRow presets;
    SegmentedChoice routing;   // INSERT | SEND

    // "ROUTING", in the same tracked caps as "Start from".
    struct Caption : juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps ("Routing"), getLocalBounds().toFloat(),
                                                  AbcTrainLookAndFeel::microFont(), AbcTrainTheme::current().textDim, 1.4f,
                                                  juce::Justification::centredLeft);
        }
    } routingCaption;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerVerbEditor)
};

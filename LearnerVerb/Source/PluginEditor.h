#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "../../shared/LearnerEditorBase.h"
#include "../../shared/KnobRow.h"
#include "../../shared/SegmentedChoice.h"
#include "../../shared/WaveformDisplay.h"
#include "EchogramView.h"

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
        presets.setActive (index);
        syncType();
        updateEchogram (true);
    }

private:
    int analysisContentHeight() const override { return 200 + 12 + 120 + 8 + 22; }
    int controlsContentHeight() const override
    {
        return (isCompact() ? 30 + 26 : 34 + 30) + 3 * rowGap() + knobRowHeight() + presetRowHeight();
    }
    void paint (juce::Graphics&) override;
    void layoutAnalysis (juce::Rectangle<int>) override;
    void layoutControls (juce::Rectangle<int>) override;
    void themeChanged() override;
    void tick() override;
    void syncType();
    void updateEchogram (bool immediately = false);

    LearnerVerbProcessor& verbProcessor;

    EchogramView echogram;
    WaveformDisplay waveform;
    juce::Label inputPeakLabel, outputPeakLabel;

    SegmentedChoice typeChoice;
    juce::Rectangle<int> typeCaptionArea, thisIsCaptionArea, thisIsTextArea;
    KnobRow knobs;
    PresetRow presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerVerbEditor)
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "../../shared/LearnerEditorBase.h"
#include "../../shared/KnobRow.h"
#include "../../shared/WaveformDisplay.h"
#include "TransferCurveView.h"
#include "../../shared/GainReductionMeter.h"

// Learner Comp: a real compressor with the analysis above and seven knobs
// below. Everything else - title row, theme, language, updates, bypass,
// the module panel - is LearnerEditorBase's (ADR 037).
class LearnerCompEditor : public LearnerEditorBase
{
public:
    explicit LearnerCompEditor (LearnerCompProcessor&);
    ~LearnerCompEditor() override;

    // Snapshot seam: a picture of every knob at its default is a picture
    // of a plugin nobody has used yet.
    void applyPresetForSnapshot (int index)
    {
        compProcessor.applyPreset (index);
        presets.setActive (index);
    }

private:
    int analysisContentHeight() const override { return 240 + 12 + 48; }
    int controlsContentHeight() const override { return knobRowHeight() + rowGap() + presetRowHeight(); }
    void layoutAnalysis (juce::Rectangle<int>) override;
    void layoutControls (juce::Rectangle<int>) override;
    void themeChanged() override;
    void tick() override;
    void updateTransferCurve();

    LearnerCompProcessor& compProcessor;

    TransferCurveView transferCurve;
    WaveformDisplay waveform;
    GainReductionMeter gainReductionMeter;
    juce::Label inputPeakLabel, outputPeakLabel;

    KnobRow knobs;
    PresetRow presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerCompEditor)
};

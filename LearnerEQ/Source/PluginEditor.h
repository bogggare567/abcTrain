#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumAnalyser.h"
#include "shared/learning/LearnerEditorBase.h"
#include "shared/ui/SegmentedChoice.h"
#include "shared/analysis/WaveformDisplay.h"

// Learner EQ: the curve is the instrument, and one row of controls follows
// whichever band is selected. The shell is LearnerEditorBase's (ADR 037).
class LearnerEQEditor : public LearnerEditorBase
{
public:
    explicit LearnerEQEditor (LearnerEQProcessor&);
    ~LearnerEQEditor() override;

private:
    int analysisContentHeight() const override { return 215 + 12 + 110 + 8 + 24; }
    int controlsContentHeight() const override { return 24 + 30 + 2 * rowGap() + (isCompact() ? 96 : 118); }
    void layoutAnalysis (juce::Rectangle<int>) override;
    void layoutControls (juce::Rectangle<int>) override;
    void themeChanged() override;
    void tick() override;
    void paintOverChildren (juce::Graphics&) override;

    void selectBand (int band);
    void pushSelectedBandToControls();
    void writeParameter (const juce::String& id, float value);
    void refreshZoneLabel();
    void pushBandsToDisplay();
    juce::String typeName (EQCoefficients::BandType) const;

    LearnerEQProcessor& eqProcessor;

    SpectrumAnalyserComponent spectrum;
    WaveformDisplay waveform;
    juce::Label inputPeakLabel, outputPeakLabel;

    juce::Label zoneLabel;
    juce::TextButton zonesButton;
    SegmentedChoice typeChoice;
    juce::Label bandLabel;

    // One chip per active band, and "+" to add one: the bands as things
    // you can count and pick, not only as dots on a curve.
    std::array<juce::TextButton, LearnerEQProcessor::maxBands> bandChips;
    juce::TextButton addBandChip { "+" };
    juce::Rectangle<int> chipRow;
    void refreshBandChips();
    juce::String formatFrequency (double hz) const;
    juce::Slider freqSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider gainSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider qSlider    { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    std::array<juce::Rectangle<int>, 3> knobCaptions;

    int selectedBand = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerEQEditor)
};

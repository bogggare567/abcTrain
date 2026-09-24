#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumAnalyser.h"
#include "shared/learning/LearnerEditorBase.h"
#include "shared/ui/SegmentedChoice.h"
#include "shared/analysis/WaveformDisplay.h"
#include "LessonPanel.h"

// Learner EQ: the curve is the instrument, and one row of controls follows
// whichever band is selected. The shell is LearnerEditorBase's (ADR 037).
class LearnerEQEditor : public LearnerEditorBase
{
public:
    explicit LearnerEQEditor (LearnerEQProcessor&);
    ~LearnerEQEditor() override;

    // Snapshot seam: the kick lesson half done - a high-pass, the body
    // lifted, band 4 on the click - so the picture shows ticks, the
    // current step and coloured nodes rather than one flat line.
    void kickLessonForSnapshot()
    {
        chooseInstrument (0);
        eqProcessor.removeBand (0);
        eqProcessor.addBand (30.0f, 0.0f, EQCoefficients::BandType::highPass);
        eqProcessor.addBand (65.0f, 3.0f, EQCoefficients::BandType::bell);
        eqProcessor.addBand (330.0f, 0.0f, EQCoefficients::BandType::bell);
        const auto click = eqProcessor.addBand (3200.0f, 4.0f, EQCoefficients::BandType::bell);
        pushBandsToDisplay();
        selectBand (click);
        refreshLesson();
    }

private:
    int analysisContentHeight() const override { return 280; }
    int controlsContentHeight() const override { return isCompact() ? 104 : 124; }
    void layoutToolbar (juce::Rectangle<int>) override;
    void layoutAnalysis (juce::Rectangle<int>) override;
    void layoutControls (juce::Rectangle<int>) override;
    void themeChanged() override;
    void tick() override;
    void paintOverChildren (juce::Graphics&) override;
    juce::Component* companionLesson() override { return &lessonPanel; }

    void selectBand (int band);
    void pushSelectedBandToControls();
    void writeParameter (const juce::String& id, float value);
    void refreshZoneLabel();
    void chooseInstrument (int index);   // -1: the general zones
    void refreshLesson();
    void pushBandsToDisplay();
    juce::String typeName (EQCoefficients::BandType) const;

    LearnerEQProcessor& eqProcessor;

    SpectrumAnalyserComponent spectrum;
    WaveformDisplay waveform;   // fed by the processor; not on screen in this layout
    LessonPanel lessonPanel;

    ChipRow instrumentChips;
    int instrument = -1;

    ChipRow typeChips;
    juce::Rectangle<int> bandCaptionArea;

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

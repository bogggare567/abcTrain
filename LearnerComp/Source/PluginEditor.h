#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "../../shared/WaveformDisplay.h"
#include "../../shared/SpectrumAnalyzer.h"
#include "../../shared/LessonController.h"
#include "../../shared/UpdateChecker.h"
#include "../../shared/AbcTrainLookAndFeel.h"
#include <array>
#include <memory>

class LearnerCompEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit LearnerCompEditor (LearnerCompProcessor&);
    ~LearnerCompEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct KnobControl
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label nameLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // Declared first so it's constructed before, and destroyed after,
    // every other Component below - see the class comment on
    // AbcTrainLookAndFeel.
    AbcTrainLookAndFeel lookAndFeel;

    LearnerCompProcessor& processor;

    juce::Label titleLabel;
    juce::Label guideLabel;
    SpectrumAnalyzerComponent spectrum;
    WaveformDisplay waveform;
    juce::Label gainReductionLabel;
    juce::Label inputPeakLabel;
    juce::Label outputPeakLabel;

    std::array<KnobControl, 7> knobs;
    juce::ToggleButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    juce::OwnedArray<juce::TextButton> presetButtons;

    juce::TextButton lessonButton { "Lesson" };
    LessonController lessonController;

    juce::TextButton updateButton { "Updates" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerCompEditor)
};

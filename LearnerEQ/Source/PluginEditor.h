#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumAnalyser.h"
#include "../../shared/LessonController.h"
#include <array>
#include <memory>

class LearnerEQEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit LearnerEQEditor (LearnerEQProcessor&);
    ~LearnerEQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct BandControls
    {
        juce::Slider freqSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider gainSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider qSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label nameLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment, gainAttachment, qAttachment;
    };

    LearnerEQProcessor& processor;
    SpectrumAnalyserComponent spectrum;
    juce::Label titleLabel;
    juce::Label guideLabel;
    std::array<BandControls, LearnerEQProcessor::numBands> bands;

    juce::TextButton lessonButton { "Lesson" };
    LessonController lessonController;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerEQEditor)
};

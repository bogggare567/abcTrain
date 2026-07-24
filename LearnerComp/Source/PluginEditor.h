#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "WaveformDisplay.h"
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

    LearnerCompProcessor& processor;

    juce::Label titleLabel;
    juce::Label guideLabel;
    WaveformDisplay waveform;
    juce::Label gainReductionLabel;
    juce::Label inputPeakLabel;
    juce::Label outputPeakLabel;

    std::array<KnobControl, 7> knobs;
    juce::ToggleButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    juce::OwnedArray<juce::TextButton> presetButtons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerCompEditor)
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class EarTrainerEditor : public juce::AudioProcessorEditor,
                          private juce::ChangeListener
{
public:
    explicit EarTrainerEditor (EarTrainerProcessor&);
    ~EarTrainerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshFromGameState();
    void bandButtonClicked (int bandIndex);

    EarTrainerProcessor& processor;

    juce::Label titleLabel;
    juce::Label instructionLabel;
    juce::Label scoreLabel;
    juce::Label feedbackLabel;
    juce::TextButton newRoundButton { "New Round" };
    juce::OwnedArray<juce::TextButton> bandButtons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarTrainerEditor)
};

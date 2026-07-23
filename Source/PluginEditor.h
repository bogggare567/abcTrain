#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Generic multiple-choice UI driven entirely by the active Game's
// interface (name/instructions/choice count/labels/feedback). Adding a
// new game to GameManager needs no changes here.
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
    void rebuildChoiceButtons();
    void choiceButtonClicked (int choiceIndex);
    void gameSelected();

    EarTrainerProcessor& processor;

    juce::Label titleLabel;
    juce::ComboBox gameSelector;
    juce::Label instructionLabel;
    juce::Label scoreLabel;
    juce::Label feedbackLabel;
    juce::TextButton newRoundButton { "New Round" };
    juce::OwnedArray<juce::TextButton> choiceButtons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarTrainerEditor)
};

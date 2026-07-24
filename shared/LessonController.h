#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MicroLesson.h"
#include <functional>

// Generic lesson-playing overlay panel: shows the current step's
// explanation text and progress, Next/Back/Close buttons, and applies
// each step's target parameter values to whatever
// AudioProcessorValueTreeState it's given. Meant to be added as a
// full-size child component in a Learner plugin's editor and toggled
// visible/invisible via a "Lesson" button - see any Learner plugin's
// PluginEditor for the integration.
//
// Deliberately does not highlight the specific control a step is
// adjusting: every target parameter already has a SliderAttachment (or
// ComboBoxAttachment) wired up in the editor, so setting it here makes
// the matching knob visibly move/rotate on its own - that visible motion
// *is* the highlight. Drawing a separate highlight outline per editor was
// considered and skipped for this pass; see ADR 005.
class LessonController : public juce::Component
{
public:
    LessonController (juce::AudioProcessorValueTreeState& stateToControl, MicroLesson lessonToPlay);

    std::function<void()> onClosed;

    void showAndStart();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void applyCurrentStep();
    void closeLesson();

    juce::AudioProcessorValueTreeState& apvts;
    MicroLesson lesson;

    juce::Label titleLabel;
    juce::Label progressLabel;
    juce::Label stepTextLabel;
    juce::TextButton previousButton { "< Back" };
    juce::TextButton nextButton { "Next >" };
    juce::TextButton closeButton { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LessonController)
};

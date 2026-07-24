#pragma once

#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

// One step of a guided lesson: explanation text plus the parameter values
// that should be set when this step becomes current. Deliberately a plain
// aggregate (no constructor) so lesson content can be written as a
// braced-init list - see any Learner plugin's lesson data file for
// examples.
struct LessonStep
{
    juce::String explanationText;
    // (parameterID, target value in real units - Hz/dB/ms/etc, not normalised)
    std::vector<std::pair<juce::String, float>> targetParameters;
};

// A sequence of LessonSteps with a cursor - start()/nextStep()/
// previousStep() move through them. Deliberately has no dependency on
// AudioProcessorValueTreeState or any UI class: applying a step's target
// parameters and displaying its text is LessonController's job (in
// shared/LessonController.h), not this class's. That split is what makes
// this directly unit-testable without constructing a processor or a
// Component - see tests/MicroLessonTest.cpp.
class MicroLesson
{
public:
    MicroLesson (juce::String lessonTitle, std::vector<LessonStep> lessonSteps)
        : title (std::move (lessonTitle)), steps (std::move (lessonSteps))
    {
    }

    void start() noexcept
    {
        currentStep = 0;
        active = true;
    }

    void stop() noexcept { active = false; }
    bool isActive() const noexcept { return active; }

    bool nextStep() noexcept
    {
        if (! active || isOnLastStep())
            return false;

        ++currentStep;
        return true;
    }

    bool previousStep() noexcept
    {
        if (! active || isOnFirstStep())
            return false;

        --currentStep;
        return true;
    }

    bool isOnFirstStep() const noexcept { return currentStep <= 0; }
    bool isOnLastStep() const noexcept { return currentStep + 1 >= (int) steps.size(); }

    int getCurrentStepIndex() const noexcept { return currentStep; }
    int getNumSteps() const noexcept { return (int) steps.size(); }

    const juce::String& getTitle() const noexcept { return title; }
    const LessonStep& getCurrentStep() const { return steps[(size_t) currentStep]; }

private:
    juce::String title;
    std::vector<LessonStep> steps;
    int currentStep = 0;
    bool active = false;
};

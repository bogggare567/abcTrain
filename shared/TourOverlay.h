#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

// A first-run walkthrough that points at the real controls.
//
// The problem it solves is narrow and real: nine exercises, an A/B pair, a
// scale, a level and a mode selector all appear at once, and nothing on
// screen says which of them is the question and which is the answer. A
// paragraph of instructions would be read by nobody; a tooltip on each
// control explains parts and never the shape.
//
// So it dims the window, cuts a hole around one actual widget, and puts one
// sentence beside it. The hole travels between steps rather than jumping,
// because watching it move is what tells you the next thing is *elsewhere*.
//
// **Offered, never imposed.** It appears once, with a decline that is as
// easy to press as the accept, and it can be left at any step. A tour you
// cannot escape is a tour that teaches people to dismiss things unread.
//
// Targets are held as SafePointer: a step whose widget has gone away is
// skipped rather than crashing, which matters because the controls it
// points at are hidden and shown as screens change.
class TourOverlay : public juce::Component,
                     private juce::Timer
{
public:
    TourOverlay();
    ~TourOverlay() override;

    void addStep (juce::Component* target, juce::String text);
    void clearSteps();

    void setStrings (juce::String next, juce::String skip, juce::String done);

    void start();
    void stop();
    bool isRunning() const noexcept { return running; }

    // Called when the tour ends, however it ended - finished or skipped.
    // The caller decides what "seen" means and where to store it.
    std::function<void()> onFinished;

    // Jumps the travel to its end state, for tools/EditorSnapshots.
    void completeAnimation();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    struct Step
    {
        juce::Component::SafePointer<juce::Component> target;
        juce::String text;
    };

    void timerCallback() override;
    void goTo (int index);
    juce::Rectangle<float> targetBoundsFor (int index) const;
    juce::Rectangle<int> captionBoundsFor (juce::Rectangle<float> hole) const;

    std::vector<Step> steps;
    int currentStep = -1;
    bool running = false;

    // The hole's drawn position, eased toward the current step's target.
    juce::Rectangle<float> drawnHole;
    juce::Rectangle<float> targetHole;
    float appear = 0.0f;

    juce::TextButton nextButton, skipButton;
    juce::String doneText { "Done" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TourOverlay)
};

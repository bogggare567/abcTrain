#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// The DVD screensaver, with soundkorb.ru where the logo used to be.
//
// It has no job beyond being pleasant to have running, and that is a good
// enough reason on its own - a trainer left open on a second screen is a
// trainer somebody comes back to. Everybody who has waited for a corner
// hit knows exactly what this is, which is the entire appeal: the one piece
// of the interface that needs no explanation.
//
// Any mouse move, click or key dismisses it, and the idle timer only counts
// while nothing is playing - a screensaver that appeared over a round in
// progress would be a bug wearing a costume.
class IdleScreensaver : public juce::Component,
                         private juce::Timer
{
public:
    IdleScreensaver();
    ~IdleScreensaver() override;

    // How long the window has to be untouched. Zero switches it off.
    void setIdleSeconds (int seconds);

    // Polled rather than pushed: the screensaver asks "is anything
    // happening" instead of every part of the app remembering to tell it.
    // One fewer thing for a new feature to forget to do.
    std::function<bool()> isBusy;

    std::function<void()> onDismissed;

    // Counts a real interaction. The editor forwards these, because a
    // component that is invisible receives no mouse events of its own.
    void noteActivity();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr const char* idleSecondsKey = "screensaverIdleSeconds";

private:
    void timerCallback() override;
    void dismiss();
    juce::Rectangle<float> markBounds() const;

    juce::String text { "soundkorb.ru" };

    juce::Point<float> position { 40.0f, 40.0f };
    juce::Point<float> velocity { 62.0f, 43.0f };   // px/sec, deliberately not a round ratio

    // Which corner colour it is on. Every bounce advances it, the way the
    // original did - that colour change is most of what people remember.
    int colourIndex = 0;

    double idleSeconds = 90.0;
    double untouchedFor = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IdleScreensaver)
};

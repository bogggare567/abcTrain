#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AmbientInstruments.h"
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

    // Three minutes. Long enough that it never interrupts somebody
    // thinking, short enough to appear while a session is left open.
    static constexpr int defaultIdleSeconds = 180;

private:
    void timerCallback() override;
    void dismiss();
    juce::Rectangle<float> markBounds() const;

    juce::String text { "soundkorb.ru" };

    // The same instruments the welcome screen drifts, on its own clock.
    // Sharing the *drawing* rather than the phase: two screens showing the
    // identical frame would read as one being a copy of the other.
    //
    // Not started at zero. At phase 0 the first scene's bell has zero gain
    // - sin(0) - so the screensaver would open on a flat horizontal line
    // and take a second to become anything. This lands mid-way through the
    // fader bank instead, which has shape from the first frame.
    double ambientPhase = 11.5;

    // A click that lands on the mark opens the site instead of just
    // dismissing. Everything else about the screensaver is deliberately
    // purposeless; this is the one thing on it worth being able to reach,
    // and a moving target you can catch is a better invitation than a
    // link in a corner.
    static constexpr const char* siteUrl = "https://soundkorb.ru";

    // True while the pointer is on the mark. It freezes there and lights
    // up, which is what makes "catch it" a thing you can actually do -
    // any mouse move used to dismiss the screensaver outright, so the
    // link was unclickable by construction.
    bool pointerOnMark = false;

    juce::Rectangle<float> markHitArea() const;

    juce::Point<float> position { 40.0f, 40.0f };
    juce::Point<float> velocity { 62.0f, 43.0f };   // px/sec, deliberately not a round ratio

    // Which corner colour it is on. Every bounce advances it, the way the
    // original did - that colour change is most of what people remember.
    int colourIndex = 0;

    double idleSeconds = 180.0;
    double untouchedFor = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IdleScreensaver)
};

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// Per-widget animation state for a shared LookAndFeel.
//
// The problem this solves: a LookAndFeel is stateless by design - one
// instance draws every button in the editor, and its draw callbacks only
// receive JUCE's instantaneous `shouldDrawButtonAsHighlighted` /
// `shouldDrawButtonAsDown` booleans. There is nowhere to keep "this
// particular button is 40% of the way into its hover transition", which is
// why the previous pass could only snap between hover states (documented as
// a known limitation in decisions/018).
//
// This registry gives the LookAndFeel that missing memory: it maps a
// Component to a set of eased values, ticks them on its own 60 Hz timer,
// and repaints only the components still in motion. A draw callback asks
// for `hoverAmount(button, isHighlighted)`, gets back a smoothly
// interpolated 0..1, and draws with it.
//
// Lifetime: entries are held as juce::Component::SafePointer, so a widget
// (or a whole editor) being destroyed mid-animation nulls the entry rather
// than leaving a dangling pointer. Dead entries are pruned on each tick.
// Message thread only - it is driven by paint() callbacks and a Timer,
// both of which are message-thread-only in JUCE.
class WidgetStateRegistry : private juce::Timer
{
public:
    WidgetStateRegistry();
    ~WidgetStateRegistry() override;

    // Returns this component's current eased hover amount (0 = idle,
    // 1 = fully hovered) and records the new target. Call from a draw
    // callback with JUCE's own instantaneous flag.
    float hoverAmount (juce::Component& component, bool isHoveredNow);

    // Same, for the pressed state. Press and release use different
    // durations on purpose: a press should register immediately, while the
    // release settles back more slowly, which is what makes a button feel
    // like it has mass rather than being a light switch.
    float pressAmount (juce::Component& component, bool isPressedNow);

private:
    struct Entry
    {
        juce::Component::SafePointer<juce::Component> component;
        float hover = 0.0f;
        float hoverTarget = 0.0f;
        float press = 0.0f;
        float pressTarget = 0.0f;
    };

    Entry& entryFor (juce::Component&);
    void timerCallback() override;

    // Linear-in-time approach toward the target, with the per-frame step
    // derived from a duration in ms. The visible easing curve comes from
    // where this value is *used* (AbcTrainTheme::Ease::out on the way into
    // a shadow radius or glow alpha), not from the interpolation itself -
    // that keeps this loop trivial and lets each caller pick its own feel.
    static float approach (float currentValue, float target, double durationMs) noexcept;

    std::vector<Entry> entries;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WidgetStateRegistry)
};

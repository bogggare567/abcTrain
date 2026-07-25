#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The contextual guide text shown while a user drags a control in the three
// Learner plugins. Replaces the plain juce::Label each editor used to keep
// in a fixed strip at the bottom.
//
// It floats in rather than appearing: over ~200 ms it rises a few pixels
// into place while fading up, and leaves the same way. The backdrop behind
// the text is a genuine Gaussian blur of whatever it is covering (via
// AbcTrainLookAndFeel::paintBlurredBackdrop), not a flat translucent
// rectangle - so the panel underneath stays legible as *context* while the
// text on top stays perfectly readable.
//
// Why blur rather than an opaque card: the guide text appears exactly while
// the user is dragging a knob, i.e. while they are watching that knob. An
// opaque panel over that area would hide the thing being explained.
class GuideTooltip : public juce::Component,
                      private juce::Timer
{
public:
    GuideTooltip();
    ~GuideTooltip() override;

    // Empty text dismisses the tooltip (animating out); non-empty shows it.
    void setText (const juce::String& newText);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    juce::String text;
    float visibility = 0.0f;      // eased 0..1
    float visibilityTarget = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuideTooltip)
};

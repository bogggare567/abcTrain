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
    //
    // `autoDismissMs` > 0 makes the card take itself away after that long.
    // Drag-driven guide text doesn't want this - the drag ending is what
    // dismisses it - but a message with no gesture behind it (a preset
    // explanation, an update result) would otherwise sit over the
    // visualisation forever. Counted on this component's own timer, so a
    // newer message replacing an older one simply restarts the countdown
    // rather than being cut short by the previous one's callback.
    void setText (const juce::String& newText, int autoDismissMs = 0);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    juce::String text;
    float visibility = 0.0f;      // eased 0..1
    float visibilityTarget = 0.0f;
    int dismissCountdownMs = 0;

    // The blurred backdrop, made once per appearance rather than per frame.
    //
    // This used to be recomputed inside paint(): a full snapshot of the
    // parent editor (which re-renders the spectrum, the waveform, every
    // knob) plus a 2D Gaussian convolution - and since the tooltip sits
    // over displays that repaint at 30 Hz, that work ran tens of times a
    // second for the whole duration of every drag. It was the single
    // biggest source of "the plugin feels laggy". The card is translucent
    // glass, so the content behind it being a moment old is invisible;
    // re-blurring it live was cost with no observable effect.
    juce::Image backdropCache;
    juce::Rectangle<int> backdropCacheArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuideTooltip)
};

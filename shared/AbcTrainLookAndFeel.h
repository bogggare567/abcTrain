#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AbcTrainTheme.h"
#include "WidgetStateRegistry.h"

// Shared look and feel for all four plugins. Every colour comes from
// AbcTrainTheme::current(), so switching to light mode is a palette swap
// plus a repaint, with no per-widget colour code anywhere.
//
// Hover and press states are genuinely *eased* rather than snapped: a
// WidgetStateRegistry member gives this otherwise-stateless class a
// per-component animation timeline (see WidgetStateRegistry.h for why that
// indirection is needed at all). Buttons lift and settle, knobs bloom and
// fade.
//
// One instance per editor (not a shared static): each editor constructs its
// own AbcTrainLookAndFeel member, calls setLookAndFeel(&laf) in its
// constructor and setLookAndFeel(nullptr) in its destructor, and declares
// the member *first* in its class so it's constructed before - and
// destroyed after - every child Component that might still reference it
// during teardown.
class AbcTrainLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AbcTrainLookAndFeel();

    // Re-reads AbcTrainTheme::current() into JUCE's colour-scheme slots.
    // Call after AbcTrainTheme::setMode(), then repaint the editor.
    void refreshFromTheme();

    static constexpr float titleFontHeight = 22.0f;
    static constexpr float bodyFontHeight = 14.0f;
    static constexpr float monoFontHeight = 16.0f;
    static constexpr float captionFontHeight = 11.0f;

    static juce::Font titleFont();
    static juce::Font monoFont();
    static juce::Font captionFont();

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    // ---- Shared painting helpers, used by components that draw
    // themselves and so never pass through a LookAndFeel callback. ----

    // The editor backdrop: a soft radial gradient plus a faint noise tooth
    // (see noiseTexture() for why the grain matters on large flat fills).
    // `tint` gives the backdrop a per-exercise colour cast. Deliberately
    // a *cast* over the theme's own base rather than a flat colour of its
    // own: the reference trainers give each exercise a whole different
    // background, which is memorable, but a saturated page behind a dark
    // instrument would fight everything drawn on it. Mixing a hint of hue
    // into the existing gradient gets the "each training has its own
    // room" feeling without touching contrast.
    static void paintPanelBackground (juce::Graphics&, juce::Rectangle<float> bounds,
                                      juce::Colour tint = juce::Colours::transparentBlack);

    // A grouped-section container: subtly raised surface, hairline border,
    // optional caption in the top-left. This is what gives related controls
    // a visual home instead of floating loose on the backdrop.
    static void paintSectionPanel (juce::Graphics&, juce::Rectangle<float> bounds,
                                   const juce::String& caption = {});

    // The quieter alternative: a caption and a hairline rule, no fill and
    // no border. Grouping by *typography and space* rather than by boxes,
    // for screens where three bordered panels chopped one window into
    // three pieces instead of organising it. Used by EarTrainer's
    // training screen; the Learner plugins keep the panels, where a
    // raised surface genuinely separates controls from data displays.
    static void paintSectionHeading (juce::Graphics&, juce::Rectangle<float> bounds,
                                     const juce::String& caption);

    // A recessed well for the spectrum/waveform displays - the inverse
    // treatment of a section panel, so data displays read as cut *into* the
    // surface while controls sit *on* it.
    static void paintDisplayWell (juce::Graphics&, juce::Rectangle<float> bounds);

    // A tiling image of low-amplitude noise. Large flat fills - even
    // gradient ones - band visibly on 8-bit displays and read as
    // synthetic; a sub-percent grain breaks the banding up and is the
    // single cheapest thing that makes a panel look like a surface rather
    // than a colour value. Cached and tiled, so it costs one small image
    // for the whole process.
    static const juce::Image& noiseTexture();
    static void overlayTexture (juce::Graphics&, juce::Rectangle<float> bounds, float strength = 1.0f);

    // Draws text with manual letter-spacing. JUCE exposes no tracking or
    // kerning control on Font/Graphics::drawText, so wide-set titles have
    // to be drawn glyph by glyph. Positive tracking on a short bold title
    // is most of what separates a considered heading from stock UI text.
    static void drawTrackedText (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                                 const juce::Font&, juce::Colour, float trackingPx,
                                 juce::Justification = juce::Justification::centredLeft);

    static float trackedTextWidth (const juce::String&, const juce::Font&, float trackingPx);

    // Blurs whatever is already in `sourceArea` of the component being
    // painted and fills `bounds` with it - a real Gaussian blur via
    // juce::ImageConvolutionKernel, used for the floating tooltip backdrop.
    static void paintBlurredBackdrop (juce::Graphics&, juce::Component& sourceComponent,
                                      juce::Rectangle<int> bounds, float blurRadius, float cornerRadius);

    // Eased interaction state for components that draw themselves and want
    // the same hover feel as LookAndFeel-drawn widgets.
    WidgetStateRegistry& getStateRegistry() noexcept { return stateRegistry; }

private:
    WidgetStateRegistry stateRegistry;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbcTrainLookAndFeel)
};

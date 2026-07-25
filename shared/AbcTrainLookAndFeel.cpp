#include "AbcTrainLookAndFeel.h"

namespace
{
    constexpr juce::uint32 windowBackground  = 0xff1e1e2e;
    constexpr juce::uint32 widgetBackground  = 0xff2a2a3a;
    constexpr juce::uint32 outlineColour     = 0xff3a3a4a;
    constexpr juce::uint32 textColour        = 0xffe0e0e0;
    constexpr juce::uint32 accentBlue        = 0xff5b9bd5;
    constexpr juce::uint32 accentOrange      = 0xffd98c5f;
}

AbcTrainLookAndFeel::AbcTrainLookAndFeel()
{
    // LookAndFeel_V4::initialiseColours() wires each of these nine slots
    // into the specific component colourIds every JUCE widget actually
    // reads (Slider::rotarySliderFillColourId and TextButton::buttonOnColourId
    // both come from highlightedFill, Slider::thumbColourId from
    // defaultFill, etc.) - setting the scheme once here is what makes the
    // accent colours apply uniformly everywhere, instead of needing a
    // setColour() call on every individual slider in every editor.
    setColourScheme (juce::LookAndFeel_V4::ColourScheme (
        windowBackground, widgetBackground, windowBackground,
        outlineColour, textColour, accentBlue,
        textColour, accentOrange, textColour));

    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (windowBackground));
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (widgetBackground));
    setColour (juce::TextEditor::outlineColourId, juce::Colour (outlineColour));
}

juce::Font AbcTrainLookAndFeel::titleFont()
{
    return juce::Font (juce::FontOptions (titleFontHeight, juce::Font::bold));
}

juce::Font AbcTrainLookAndFeel::monoFont()
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), monoFontHeight, juce::Font::plain));
}

juce::Font AbcTrainLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (bodyFontHeight));
}

juce::Font AbcTrainLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions (bodyFontHeight));
}

juce::Font AbcTrainLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (bodyFontHeight));
}

juce::Font AbcTrainLookAndFeel::getPopupMenuFont()
{
    return juce::Font (juce::FontOptions (bodyFontHeight));
}

juce::Font AbcTrainLookAndFeel::getAlertWindowTitleFont()
{
    return titleFont();
}

juce::Font AbcTrainLookAndFeel::getAlertWindowMessageFont()
{
    return juce::Font (juce::FontOptions (bodyFontHeight));
}

void AbcTrainLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                                 bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    constexpr float cornerRadius = 7.0f;

    // `backgroundColour` already folds in TextButton::buttonColourId (or
    // buttonOnColourId when toggled on) - starting from it rather than a
    // hardcoded fill is what lets EarTrainer's per-choice-button colours
    // (correct/wrong/darkgrey, set via setColour() at answer time) still
    // show through, instead of every button looking identical regardless
    // of what a caller explicitly asked for.
    auto fill = backgroundColour;

    // Shadow grows on hover and shrinks on press - a "lift/settle" cue.
    // A shared, stateless LookAndFeel has no per-button animation state to
    // work with, so this responds immediately to JUCE's own highlighted/
    // down flags rather than easing between sizes - still a real depth
    // cue, just not an animated one (see decisions/018).
    float shadowAlpha = 0.28f;
    int shadowRadius = 5;
    juce::Point<int> shadowOffset (0, 2);

    if (shouldDrawButtonAsDown)
    {
        fill = fill.darker (0.08f);
        shadowAlpha = 0.12f;
        shadowRadius = 2;
        shadowOffset = { 0, 1 };
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        fill = fill.brighter (0.12f);
        shadowAlpha = 0.45f;
        shadowRadius = 9;
        shadowOffset = { 0, 3 };
    }

    juce::Path shape;
    shape.addRoundedRectangle (bounds, cornerRadius);

    juce::DropShadow shadow (juce::Colours::black.withAlpha (shadowAlpha), shadowRadius, shadowOffset);
    shadow.drawForPath (g, shape);

    // Soft top-to-bottom gradient instead of a flat fill - a plain solid
    // rectangle is what read as "childish" before (see decisions/014);
    // a subtle sheen reads as a physical, slightly convex surface instead.
    juce::ColourGradient gradient (fill.brighter (0.06f), bounds.getX(), bounds.getY(),
                                    fill.darker (0.05f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, cornerRadius);

    g.setColour (juce::Colour (outlineColour).withAlpha (0.9f));
    g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);
}

void AbcTrainLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPosProportional, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    constexpr float trackThickness = 2.5f;
    const auto fillColour = slider.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (outlineColour));
    g.strokePath (track, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, angle, true);

    // A soft glow behind the value arc while the knob is actively being
    // touched - drawn as two progressively-wider, more-transparent copies
    // of the same arc underneath the crisp one, the standard cheap way to
    // fake a glow without a real blur filter.
    if (slider.isMouseOverOrDragging())
    {
        g.setColour (fillColour.withAlpha (0.18f));
        g.strokePath (valueArc, juce::PathStrokeType (trackThickness + 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (fillColour.withAlpha (0.30f));
        g.strokePath (valueArc, juce::PathStrokeType (trackThickness + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (fillColour);
    g.strokePath (valueArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path pointer;
    const auto pointerLength = radius * 0.65f;
    pointer.startNewSubPath (centre);
    pointer.lineTo (centre.getPointOnCircumference (pointerLength, angle));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.strokePath (pointer, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knob cap: a small gradient disc (lighter at the top) plus a faint
    // shadow, rather than a flat fill, so it reads as a slightly domed
    // physical cap rather than a painted circle.
    const auto capBounds = juce::Rectangle<float> (radius * 0.64f, radius * 0.64f).withCentre (centre);

    juce::Path capPath;
    capPath.addEllipse (capBounds);
    juce::DropShadow capShadow (juce::Colours::black.withAlpha (0.35f), 4, { 0, 1 });
    capShadow.drawForPath (g, capPath);

    juce::ColourGradient capGradient (juce::Colour (widgetBackground).brighter (0.15f), capBounds.getX(), capBounds.getY(),
                                       juce::Colour (widgetBackground).darker (0.15f), capBounds.getX(), capBounds.getBottom(), false);
    g.setGradientFill (capGradient);
    g.fillEllipse (capBounds);
}

void AbcTrainLookAndFeel::paintPanelBackground (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // A gentle radial gradient (a touch lighter at the top, where the
    // title row sits, fading to the base colour toward the edges) instead
    // of one flat fill - the difference is subtle by design (this is a
    // working tool, not a hero image) but it's what keeps a large solid
    // dark rectangle from reading as flat/cheap.
    const auto centre = juce::Point<float> (bounds.getCentreX(), bounds.getY() + bounds.getHeight() * 0.15f);
    const auto radius = juce::jmax (bounds.getWidth(), bounds.getHeight()) * 0.9f;

    juce::ColourGradient gradient (juce::Colour (windowBackground).brighter (0.035f), centre.x, centre.y,
                                    juce::Colour (windowBackground), centre.x, centre.y + radius, true);
    g.setGradientFill (gradient);
    g.fillRect (bounds);
}

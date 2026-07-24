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
    constexpr float cornerRadius = 6.0f;

    // `backgroundColour` already folds in TextButton::buttonColourId (or
    // buttonOnColourId when toggled on) - starting from it rather than a
    // hardcoded fill is what lets EarTrainer's per-choice-button colours
    // (correct/wrong/darkgrey, set via setColour() at answer time) still
    // show through, instead of every button looking identical regardless
    // of what a caller explicitly asked for.
    auto fill = backgroundColour;
    if (shouldDrawButtonAsDown)
        fill = fill.brighter (0.25f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter (0.12f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, cornerRadius);

    g.setColour (juce::Colour (outlineColour));
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

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (outlineColour));
    g.strokePath (track, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (valueArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path pointer;
    const auto pointerLength = radius * 0.65f;
    pointer.startNewSubPath (centre);
    pointer.lineTo (centre.getPointOnCircumference (pointerLength, angle));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.strokePath (pointer, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (juce::Colour (widgetBackground));
    g.fillEllipse (juce::Rectangle<float> (radius * 0.32f, radius * 0.32f).withCentre (centre));
}

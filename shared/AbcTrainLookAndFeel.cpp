#include "AbcTrainLookAndFeel.h"
#include <cmath>

namespace
{
    // Process-wide, like the palette and the wallpaper, and for the same
    // reason: two editors open at once must agree.
    float sharedTextScale = 1.0f;
}

namespace
{
    using namespace AbcTrainTheme;

    // Shadow helper: every elevated surface in the UI casts its shadow
    // through here, so the tint/strength difference between dark mode
    // (tight and near-black) and light mode (wide, soft, cool grey) is
    // applied once rather than at a dozen call sites.
    void dropShadowForPath (juce::Graphics& g, const juce::Path& path, float alpha, int radius,
                            juce::Point<int> offset)
    {
        const auto& theme = current();
        juce::DropShadow shadow (theme.shadow.withAlpha (alpha * theme.shadowStrength),
                                  radius, offset);
        shadow.drawForPath (g, path);
    }

    // Layered fake blur: N progressively wider, fainter strokes of the same
    // path. Cheaper than a real convolution and, for a thin bright arc on a
    // dark ground, visually indistinguishable from one.
    void glowPath (juce::Graphics& g, const juce::Path& path, juce::Colour colour,
                   float baseThickness, float intensity)
    {
        if (intensity <= 0.001f)
            return;

        constexpr int layers = 3;
        for (int i = layers; i >= 1; --i)
        {
            const auto spread = 2.5f * (float) i;
            const auto alpha = 0.16f * intensity / (float) i;
            g.setColour (colour.withAlpha (alpha));
            g.strokePath (path, juce::PathStrokeType (baseThickness + spread,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }
    }
}

AbcTrainLookAndFeel::AbcTrainLookAndFeel()
{
    refreshFromTheme();
}

void AbcTrainLookAndFeel::refreshFromTheme (juce::Colour accentOverride)
{
    const auto& t = current();
    const auto accent = accentOverride.isTransparent() ? t.accent : accentOverride;

    // LookAndFeel_V4::initialiseColours() wires each of these nine slots
    // into the specific component colourIds every JUCE widget actually
    // reads (Slider::rotarySliderFillColourId and TextButton::buttonOnColourId
    // both come from highlightedFill, Slider::thumbColourId from
    // defaultFill, etc.) - setting the scheme is what makes the accent
    // colours apply uniformly everywhere, instead of needing a setColour()
    // call on every individual slider in every editor.
    setColourScheme (juce::LookAndFeel_V4::ColourScheme (
        t.windowBackground, t.widgetBackground, t.panelBackground,
        t.outline, t.text, accent,
        t.text, t.accentWarm, t.text));

    setColour (juce::ResizableWindow::backgroundColourId, t.windowBackground);
    setColour (juce::TextEditor::backgroundColourId, t.widgetBackground);
    setColour (juce::TextEditor::outlineColourId, t.outline);
    setColour (juce::Label::textColourId, t.text);
    setColour (juce::ComboBox::backgroundColourId, t.widgetBackground);
    setColour (juce::ComboBox::outlineColourId, t.outline);
    setColour (juce::ComboBox::textColourId, t.text);
    setColour (juce::ComboBox::arrowColourId, t.textDim);
    setColour (juce::PopupMenu::backgroundColourId, t.panelBackground);
    setColour (juce::PopupMenu::textColourId, t.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, t.textBright);
    setColour (juce::TextButton::buttonColourId, t.widgetBackground);
    setColour (juce::TextButton::textColourOffId, t.text);
    setColour (juce::TextButton::textColourOnId, t.textBright);
    setColour (juce::ToggleButton::textColourId, t.text);
    setColour (juce::ToggleButton::tickColourId, t.accentWarm);
    setColour (juce::HyperlinkButton::textColourId, accent);

    // The rotary value arc, explicitly.
    //
    // Found by rendering the editors (tools/EditorSnapshots): every knob in
    // all three Learner plugins drew its arc in amber, including LearnerEQ,
    // whose whole identity is blue. LookAndFeel_V4 maps
    // Slider::rotarySliderFillColourId from the scheme's *highlightedFill*
    // slot, which this class fills with accentWarm - the "you are touching
    // this" colour - not with the accent. So the family colour reached the
    // spectrum, the waveform, the icon and the backdrop, and stopped
    // exactly at the controls. Setting the id directly is narrower than
    // moving accent into highlightedFill, which would also have restyled
    // every TextButton's on-state across all four plugins.
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, t.outline);
    setColour (juce::Slider::thumbColourId, accent);
    setColour (juce::Slider::trackColourId, accent.withAlpha (0.55f));

    // The value readout under every rotary knob. JUCE's default draws it
    // as a bordered, filled text field, which puts eighteen little boxes
    // across a Learner editor and makes the numbers read as inputs rather
    // than as part of the knob. Transparent well and border; the number
    // itself gets the bright text colour, because in a plugin whose job is
    // teaching, the *value* is the thing you are supposed to be learning
    // to associate with a sound.
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxTextColourId, t.textBright);
    setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.35f));
}

// ---------------------------------------------------------------- fonts

juce::Label* AbcTrainLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (monoFont().withHeight (13.0f));
    label->setJustificationType (juce::Justification::centred);
    return label;
}

void AbcTrainLookAndFeel::setTextScale (float newScale) noexcept
{
    sharedTextScale = juce::jlimit (0.8f, 1.4f, newScale);
}


// The interface typeface.
//
// Everything here used to ask JUCE for its default sans, which on macOS is
// a 1990s system face and on Linux is whatever happened to be installed.
// It is the single loudest "this was not designed" signal a UI can send,
// and it is free to fix: every platform ships a good interface font, they
// are simply not the *default* one.
//
// A licensed typeface of our own is still the right long-term answer (see
// docs/decisions/009), but shipping one means paying for it and embedding
// it. Asking for the best face the machine already has costs nothing and
// gets most of the way there.
static juce::String chosenTypefaceOverride;

void AbcTrainLookAndFeel::setTypefaceName (const juce::String& name)
{
    chosenTypefaceOverride = name;
}

juce::String AbcTrainLookAndFeel::getTypefaceName()
{
    return chosenTypefaceOverride;
}

juce::StringArray AbcTrainLookAndFeel::availableTypefaceNames()
{
    static const juce::StringArray all = []
    {
        juce::StringArray names { "System" };
        names.addArray (juce::Font::findAllTypefaceNames());
        names.removeDuplicates (true);
        return names;
    }();

    return all;
}

static juce::String interfaceTypefaceName()
{
    if (chosenTypefaceOverride.isNotEmpty() && chosenTypefaceOverride != "System")
        return chosenTypefaceOverride;

    // findAllTypefaceNames() enumerates the whole system font book, which
    // is slow enough to matter in a paint callback - so it happens once.
    static const juce::String chosen = []
    {
        const juce::StringArray preferred
        {
           #if JUCE_MAC
            "SF Pro Text", "SF Pro Display", ".AppleSystemUIFont", "Helvetica Neue", "Avenir Next",
           #elif JUCE_WINDOWS
            "Segoe UI Variable Text", "Segoe UI",
           #else
            "Inter", "Ubuntu", "Noto Sans", "Cantarell", "DejaVu Sans",
           #endif
        };

        const auto available = juce::Font::findAllTypefaceNames();

        for (const auto& name : preferred)
            if (available.contains (name))
                return name;

        return juce::Font::getDefaultSansSerifFontName();
    }();

    return chosen;
}


float AbcTrainLookAndFeel::getTextScale() noexcept
{
    return sharedTextScale;
}

juce::Font AbcTrainLookAndFeel::displayFont()
{
    // Near-zero tracking: at this size the default letter spacing reads as
    // loose, and a wordmark that looks loose looks like body text scaled up.
    return juce::Font (juce::FontOptions (interfaceTypefaceName(),
                                           displayFontHeight * sharedTextScale, juce::Font::bold));
}

juce::Font AbcTrainLookAndFeel::headingFont()
{
    return juce::Font (juce::FontOptions (interfaceTypefaceName(),
                                           headingFontHeight * sharedTextScale, juce::Font::bold));
}

juce::Font AbcTrainLookAndFeel::bodyFont()
{
    return juce::Font (juce::FontOptions (interfaceTypefaceName(),
                                           bodyFontHeight * sharedTextScale, juce::Font::plain));
}

juce::Font AbcTrainLookAndFeel::labelFont()
{
    return juce::Font (juce::FontOptions (interfaceTypefaceName(),
                                           labelFontHeight * sharedTextScale, juce::Font::plain));
}

juce::Font AbcTrainLookAndFeel::microFont()
{
    return juce::Font (juce::FontOptions (interfaceTypefaceName(),
                                           microFontHeight * sharedTextScale, juce::Font::plain));
}

juce::Font AbcTrainLookAndFeel::titleFont()
{
    return juce::Font (juce::FontOptions (interfaceTypefaceName(),
                                       titleFontHeight * sharedTextScale, juce::Font::bold));
}

juce::Font AbcTrainLookAndFeel::monoFont()
{
    // Monospaced for every numeric readout: peak meters, dB values and
    // scores all change continuously, and a proportional font makes them
    // jitter horizontally as digits change width. This is the closest JUCE
    // gets to tabular figures without shipping a licensed typeface.
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                           monoFontHeight * sharedTextScale, juce::Font::plain));
}

juce::Font AbcTrainLookAndFeel::captionFont()
{
    return juce::Font (juce::FontOptions (interfaceTypefaceName(),
                                       captionFontHeight * sharedTextScale, juce::Font::plain));
}

juce::Font AbcTrainLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (interfaceTypefaceName(),
                                       bodyFontHeight * sharedTextScale, juce::Font::plain));
}

juce::Font AbcTrainLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions (interfaceTypefaceName(),
                                       bodyFontHeight * sharedTextScale, juce::Font::plain));
}

juce::Font AbcTrainLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (interfaceTypefaceName(),
                                       bodyFontHeight * sharedTextScale, juce::Font::plain));
}

juce::Font AbcTrainLookAndFeel::getPopupMenuFont()
{
    return juce::Font (juce::FontOptions (interfaceTypefaceName(),
                                       bodyFontHeight * sharedTextScale, juce::Font::plain));
}

juce::Font AbcTrainLookAndFeel::getAlertWindowTitleFont()
{
    return titleFont();
}

juce::Font AbcTrainLookAndFeel::getAlertWindowMessageFont()
{
    return juce::Font (juce::FontOptions (interfaceTypefaceName(),
                                       bodyFontHeight * sharedTextScale, juce::Font::plain));
}

// -------------------------------------------------------------- buttons

void AbcTrainLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool shouldDrawButtonAsDown)
{
    const auto& t = current();

    // Eased hover/press, not JUCE's raw booleans - this is what makes the
    // lift and settle feel weighted. See WidgetStateRegistry.h.
    const auto hover = Ease::out (stateRegistry.hoverAmount (button, shouldDrawButtonAsHighlighted));
    const auto press = Ease::out (stateRegistry.pressAmount (button, shouldDrawButtonAsDown));

    // The button visually sinks by up to 1px while held, and its shadow
    // collapses underneath it: the two together read as the surface being
    // pushed toward the panel rather than just changing colour.
    const auto sink = press * 1.0f;
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f).translated (0.0f, sink);

    // `backgroundColour` already folds in TextButton::buttonColourId (or
    // buttonOnColourId when toggled on) - starting from it rather than a
    // hardcoded fill is what lets EarTrainer's per-choice colours (set via
    // setColour() at answer time) still show through, instead of every
    // button looking identical regardless of what a caller asked for.
    auto fill = backgroundColour
                    .brighter (0.14f * hover)
                    .darker (0.10f * press);

    juce::Path shape;
    shape.addRoundedRectangle (bounds, Radius::button);

    // Shadow interpolates between resting, lifted and pressed states.
    const auto shadowAlpha  = juce::jmap (hover, 0.26f, 0.46f) * (1.0f - press * 0.7f);
    const auto shadowRadius = (int) std::round (juce::jmap (hover, 5.0f, 10.0f) * (1.0f - press * 0.65f));
    const auto shadowDrop   = (int) std::round (juce::jmap (hover, 2.0f, 4.0f) * (1.0f - press * 0.75f));
    dropShadowForPath (g, shape, shadowAlpha, juce::jmax (1, shadowRadius), { 0, juce::jmax (0, shadowDrop) });

    juce::ColourGradient gradient (fill.brighter (0.07f), bounds.getX(), bounds.getY(),
                                    fill.darker (0.06f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, Radius::button);

    // A hairline of light along the top edge only. Real raised surfaces
    // catch light on their upper bevel; a uniform border does not.
    g.setColour (t.textBright.withAlpha (0.05f + 0.06f * hover));
    g.drawLine (bounds.getX() + Radius::button, bounds.getY() + 0.5f,
                bounds.getRight() - Radius::button, bounds.getY() + 0.5f, 1.0f);

    g.setColour (t.outline.withAlpha (0.9f).brighter (0.25f * hover));
    g.drawRoundedRectangle (bounds, Radius::button, 1.0f);
}

void AbcTrainLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    const auto& t = current();
    const auto hover = Ease::out (stateRegistry.hoverAmount (button, shouldDrawButtonAsHighlighted));
    juce::ignoreUnused (shouldDrawButtonAsDown);

    // A sliding pill switch rather than JUCE's stock tickbox: this is the
    // Bypass control on all three Learner plugins, where "is it on right
    // now" needs to read at a glance from across a room.
    const auto bounds = button.getLocalBounds().toFloat();
    const auto switchHeight = juce::jmin (18.0f, bounds.getHeight());
    const auto switchWidth = switchHeight * 1.85f;
    const auto switchBounds = juce::Rectangle<float> (switchWidth, switchHeight)
                                   .withY (bounds.getCentreY() - switchHeight * 0.5f)
                                   .withX (bounds.getX());

    const auto on = button.getToggleState();
    const auto trackColour = on ? t.accentWarm.withAlpha (0.85f)
                                : t.widgetBackground.darker (0.15f);

    juce::Path track;
    track.addRoundedRectangle (switchBounds, switchHeight * 0.5f);
    dropShadowForPath (g, track, 0.22f, 4, { 0, 1 });

    g.setColour (trackColour);
    g.fillRoundedRectangle (switchBounds, switchHeight * 0.5f);
    g.setColour (t.outline.withAlpha (0.8f));
    g.drawRoundedRectangle (switchBounds, switchHeight * 0.5f, 1.0f);

    const auto knobDiameter = switchHeight - 4.0f;
    const auto travel = switchBounds.getWidth() - knobDiameter - 4.0f;
    const auto knobX = switchBounds.getX() + 2.0f + (on ? travel : 0.0f);
    const auto knobBounds = juce::Rectangle<float> (knobX, switchBounds.getY() + 2.0f,
                                                     knobDiameter, knobDiameter);

    juce::Path knob;
    knob.addEllipse (knobBounds);
    dropShadowForPath (g, knob, 0.35f + 0.15f * hover, 4, { 0, 1 });

    juce::ColourGradient knobGradient (t.textBright, knobBounds.getX(), knobBounds.getY(),
                                        t.textBright.darker (0.18f), knobBounds.getX(), knobBounds.getBottom(), false);
    g.setGradientFill (knobGradient);
    g.fillEllipse (knobBounds);

    const auto textArea = bounds.withTrimmedLeft (switchWidth + (float) Spacing::small);
    g.setColour (button.findColour (juce::ToggleButton::textColourId)
                       .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
    g.setFont (juce::Font (juce::FontOptions (bodyFontHeight)));
    g.drawText (button.getButtonText(), textArea, juce::Justification::centredLeft, true);
}

// -------------------------------------------------------------- sliders

void AbcTrainLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPosProportional, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider& slider)
{
    const auto& t = current();
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    // The drawn position lags the real one, so a value that arrives on its
    // own - a lesson step, a preset, a check revealing its answer - is a
    // knob you watch *travel* rather than a number that teleported. While
    // the pointer is on it the position snaps: lag under your own hand is
    // not weight, it is latency.
    const auto drawnPosition = stateRegistry.trackValue (slider, 0, sliderPosProportional,
                                                          Duration::transition,
                                                          slider.isMouseButtonDown());
    const auto angle = rotaryStartAngle + drawnPosition * (rotaryEndAngle - rotaryStartAngle);

    const auto touch = Ease::out (stateRegistry.hoverAmount (slider, slider.isMouseOverOrDragging()));

    // The knob swells very slightly under the cursor - about 3% - and its
    // value arc thickens. Both are below the threshold of being noticed as
    // motion, and above the threshold of the control feeling alive.
    const auto scale = 1.0f + 0.03f * touch;
    const auto ringRadius = radius * scale;
    const auto trackThickness = 2.5f + 0.9f * touch;

    const auto fillColour = slider.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (t.outline);
    g.strokePath (track, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                            rotaryStartAngle, angle, true);

    glowPath (g, valueArc, fillColour, trackThickness, touch);

    // The value arc is a gradient across the knob rather than one flat
    // colour - it picks up the warm accent toward the top of its travel,
    // so "how far up is this" is readable from colour as well as angle.
    juce::ColourGradient arcGradient (fillColour.darker (0.25f), bounds.getX(), bounds.getBottom(),
                                       fillColour.brighter (0.15f), bounds.getRight(), bounds.getY(), false);
    g.setGradientFill (arcGradient);
    g.strokePath (valueArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    // Knob cap: a gradient disc lit from the top-left, over its own
    // shadow, so it reads as a domed physical cap rather than a flat disc.
    const auto capDiameter = radius * 1.28f * scale;
    const auto capBounds = juce::Rectangle<float> (capDiameter, capDiameter).withCentre (centre);

    juce::Path capPath;
    capPath.addEllipse (capBounds);
    dropShadowForPath (g, capPath, 0.34f + 0.12f * touch, 5 + (int) (3.0f * touch), { 0, 2 });

    juce::ColourGradient capGradient (t.widgetBackground.brighter (0.20f),
                                       capBounds.getX() + capBounds.getWidth() * 0.25f, capBounds.getY(),
                                       t.widgetBackground.darker (0.22f),
                                       capBounds.getCentreX(), capBounds.getBottom(), false);
    g.setGradientFill (capGradient);
    g.fillEllipse (capBounds);

    g.setColour (t.outline.withAlpha (0.7f));
    g.drawEllipse (capBounds, 1.0f);

    // Pointer stops short of the cap edge instead of starting at dead
    // centre - a full-radius spoke looks like a clock hand, a short
    // indicator at the rim looks like a control surface.
    juce::Path pointer;
    const auto capRadius = capDiameter * 0.5f;
    pointer.startNewSubPath (centre.getPointOnCircumference (capRadius * 0.45f, angle));
    pointer.lineTo (centre.getPointOnCircumference (capRadius * 0.88f, angle));
    g.setColour (t.textBright.withAlpha (0.9f));
    g.strokePath (pointer, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
}

void AbcTrainLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float minSliderPos, float maxSliderPos,
                                             juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos);

    const auto& t = current();
    const auto touch = Ease::out (stateRegistry.hoverAmount (slider, slider.isMouseOverOrDragging()));
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto isHorizontal = (style == juce::Slider::LinearHorizontal
                               || style == juce::Slider::LinearBar);

    const auto trackThickness = 5.0f + 1.5f * touch;
    const auto fillColour = slider.findColour (juce::Slider::thumbColourId);

    juce::Rectangle<float> track;
    if (isHorizontal)
        track = juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - trackThickness * 0.5f,
                                         bounds.getWidth(), trackThickness);
    else
        track = juce::Rectangle<float> (bounds.getCentreX() - trackThickness * 0.5f, bounds.getY(),
                                         trackThickness, bounds.getHeight());

    // Recessed track: dark at the top edge, lighter at the bottom, i.e.
    // the opposite gradient direction from a raised button. That inversion
    // is what makes a groove read as cut into the surface.
    juce::ColourGradient trackGradient (t.displayBackground.darker (0.2f), track.getX(), track.getY(),
                                         t.widgetBackground.brighter (0.05f), track.getX(), track.getBottom(), false);
    g.setGradientFill (trackGradient);
    g.fillRoundedRectangle (track, trackThickness * 0.5f);

    auto filled = track;
    if (isHorizontal)
        filled = filled.withRight (sliderPos);
    else
        filled = filled.withTop (sliderPos);

    juce::ColourGradient fillGradient (fillColour.darker (0.2f), filled.getX(), filled.getBottom(),
                                        fillColour.brighter (0.12f), filled.getRight(), filled.getY(), false);
    g.setGradientFill (fillGradient);
    g.fillRoundedRectangle (filled, trackThickness * 0.5f);

    // A cap, not a dot.
    //
    // It was a circle of radius 6 sitting on a 5px track - barely larger
    // than the groove it rides in, which is a dot marking a position
    // rather than a handle you take hold of. Every hardware fader and
    // every plugin that imitates one has a cap that is clearly wider
    // across the travel than the track is thick, and clearly longer
    // across the other axis: that shape is what says "grab here", and it
    // is also what your finger or cursor actually has to hit.
    const auto capThickness = 13.0f + 2.0f * touch;   // across the travel
    const auto capLength = 22.0f + 3.0f * touch;      // across the track
    const auto thumbCentre = isHorizontal ? juce::Point<float> (sliderPos, track.getCentreY())
                                          : juce::Point<float> (track.getCentreX(), sliderPos);

    const auto thumbBounds = (isHorizontal
                                  ? juce::Rectangle<float> (capThickness, capLength)
                                  : juce::Rectangle<float> (capLength, capThickness))
                                 .withCentre (thumbCentre);

    const auto capRadius = 3.5f;

    if (touch > 0.001f)
    {
        g.setColour (fillColour.withAlpha (0.20f * touch));
        g.fillRoundedRectangle (thumbBounds.expanded (4.0f), capRadius + 3.0f);
    }

    juce::Path thumbPath;
    thumbPath.addRoundedRectangle (thumbBounds, capRadius);
    dropShadowForPath (g, thumbPath, 0.38f, 6, { 0, 2 });

    juce::ColourGradient thumbGradient (t.textBright, thumbBounds.getX(), thumbBounds.getY(),
                                         t.textBright.darker (0.26f), thumbBounds.getX(), thumbBounds.getBottom(), false);
    g.setGradientFill (thumbGradient);
    g.fillRoundedRectangle (thumbBounds, capRadius);

    // One line across the cap, where a fader's grip line goes. It is also
    // the only thing that tells you which way the cap is oriented.
    g.setColour (t.displayBackground.withAlpha (0.35f));

    if (isHorizontal)
        g.fillRect (thumbBounds.getCentreX() - 0.5f, thumbBounds.getY() + 5.0f, 1.0f, thumbBounds.getHeight() - 10.0f);
    else
        g.fillRect (thumbBounds.getX() + 5.0f, thumbBounds.getCentreY() - 0.5f, thumbBounds.getWidth() - 10.0f, 1.0f);
}

// ----------------------------------------------------------- combo/menu

void AbcTrainLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                         int buttonX, int buttonY, int buttonW, int buttonH,
                                         juce::ComboBox& box)
{
    juce::ignoreUnused (isButtonDown, buttonX, buttonY, buttonW, buttonH);

    const auto& t = current();
    const auto hover = Ease::out (stateRegistry.hoverAmount (box, box.isMouseOver()));
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (1.0f);

    juce::Path shape;
    shape.addRoundedRectangle (bounds, Radius::button);
    dropShadowForPath (g, shape, 0.20f + 0.14f * hover, 5 + (int) (3.0f * hover), { 0, 2 });

    const auto fill = t.widgetBackground.brighter (0.10f * hover);
    juce::ColourGradient gradient (fill.brighter (0.06f), bounds.getX(), bounds.getY(),
                                    fill.darker (0.05f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, Radius::button);

    g.setColour (t.outline.brighter (0.25f * hover));
    g.drawRoundedRectangle (bounds, Radius::button, 1.0f);

    // Chevron rather than JUCE's filled triangle: lighter visual weight,
    // and it matches the line-art vocabulary of shared/AppIcons.
    const auto arrowArea = juce::Rectangle<float> (bounds.getRight() - 22.0f, bounds.getY(),
                                                    16.0f, bounds.getHeight());
    const auto cx = arrowArea.getCentreX();
    const auto cy = arrowArea.getCentreY();

    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.5f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (t.textDim.brighter (0.4f * hover));
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
}

void AbcTrainLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto& t = current();
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);

    g.fillAll (juce::Colours::transparentBlack);

    juce::Path shape;
    shape.addRoundedRectangle (bounds, Radius::panel);
    dropShadowForPath (g, shape, 0.5f, 14, { 0, 5 });

    g.setColour (t.panelBackground);
    g.fillRoundedRectangle (bounds, Radius::panel);
    overlayTexture (g, bounds, 0.7f);

    g.setColour (t.outline);
    g.drawRoundedRectangle (bounds, Radius::panel, 1.0f);
}

// ------------------------------------------------------ shared painters

void AbcTrainLookAndFeel::paintPanelBackground (juce::Graphics& g, juce::Rectangle<float> bounds,
                                                 juce::Colour tint)
{
    const auto& t = current();

    // A player-supplied image wins over everything below: if someone has
    // gone to the trouble of choosing a background, the app's own gradient
    // is not what they want to look at. Drawn to fill and then damped by a
    // scrim, because a photograph at full strength under 12px labels is
    // unreadable no matter how nice the photograph is.
    if (const auto& custom = customBackground(); custom.isValid())
    {
        g.drawImage (custom, bounds, juce::RectanglePlacement::fillDestination);

        g.setColour (t.windowBackground.withAlpha (customBackgroundScrim()));
        g.fillRect (bounds);

        overlayTexture (g, bounds, 0.5f);
        return;
    }

    // Each exercise's own room.
    //
    // The two modes need very different amounts, and matching them was a
    // mistake worth recording: 26% of amber mixed into a warm off-white
    // turns the whole window tan - it reads as a stain rather than as a
    // room - while the same 26% of the same colour against a near-black is
    // barely a tilt. A light page has far less headroom before a tint
    // becomes the subject.
    const auto tintStrength = t.mode == Mode::light ? 0.10f : 0.32f;
    const auto base = tint.isTransparent() ? t.windowBackground
                                           : t.windowBackground.interpolatedWith (tint.withAlpha (1.0f), tintStrength);

    // A gentle radial gradient centred above the title row, so the top of
    // the window feels lit and the corners fall away.
    const auto centre = juce::Point<float> (bounds.getCentreX(), bounds.getY() + bounds.getHeight() * 0.15f);
    const auto radius = juce::jmax (bounds.getWidth(), bounds.getHeight()) * 0.9f;

    const auto lift = t.mode == Mode::light ? 0.5f : 0.06f;
    juce::ColourGradient gradient (base.brighter (lift), centre.x, centre.y,
                                    base.darker (t.mode == Mode::light ? 0.05f : 0.06f),
                                    centre.x, centre.y + radius, true);
    g.setGradientFill (gradient);
    g.fillRect (bounds);

    overlayTexture (g, bounds);
}

namespace
{
    // Process-wide, like the palette itself and for the same reason: two
    // editors open at once must not disagree about the wallpaper. Message
    // thread only.
    juce::Image sharedCustomBackground;
    float sharedCustomScrim = 0.55f;
}

const juce::Image& AbcTrainLookAndFeel::customBackground()
{
    return sharedCustomBackground;
}

float AbcTrainLookAndFeel::customBackgroundScrim()
{
    return sharedCustomScrim;
}

void AbcTrainLookAndFeel::setCustomBackground (juce::Image image, float scrim)
{
    sharedCustomBackground = std::move (image);
    sharedCustomScrim = juce::jlimit (0.0f, 0.95f, scrim);
}

void AbcTrainLookAndFeel::paintSectionPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                                              const juce::String& caption)
{
    const auto& t = current();

    // One implementation of "a raised card", used here and by every other
    // panel in the four plugins. Two near-identical hand-rolled versions of
    // this is how the sections stopped matching each other.
    paintRaisedCard (g, bounds);

    if (caption.isNotEmpty())
    {
        // Small, dim, wide-tracked, uppercase: a section label should be
        // findable when looked for and invisible when not.
        const auto captionArea = bounds.reduced ((float) Spacing::medium, 0.0f)
                                        .withHeight (16.0f)
                                        .withY (bounds.getY() + (float) Spacing::small);
        drawTrackedText (g, caption.toUpperCase(), captionArea, captionFont(),
                         t.textDim.withAlpha (0.75f), 1.2f, juce::Justification::centredLeft);

        const auto lineY = captionArea.getBottom() + 3.0f;
        g.setColour (t.divider);
        g.drawLine (bounds.getX() + (float) Spacing::medium, lineY,
                    bounds.getRight() - (float) Spacing::medium, lineY, 1.0f);
    }
}

void AbcTrainLookAndFeel::paintRaisedCard (juce::Graphics& g, juce::Rectangle<float> bounds,
                                            float elevation)
{
    const auto& t = current();

    juce::Path shape;
    shape.addRoundedRectangle (bounds, AbcTrainTheme::Radius::panel);

    // The shadow scales with elevation and stays *under* the shape, never
    // around it: a shadow on all four sides reads as a glow, and a glow
    // says "selected", not "raised".
    dropShadowForPath (g, shape, 0.34f * t.shadowStrength * elevation,
                        (int) (14.0f * elevation), { 0, (int) (3.0f * elevation) });

    // Lit from above. Two stops rather than a flat fill, and the difference
    // is small enough that it reads as light rather than as a gradient.
    juce::ColourGradient face (t.panelBackground.brighter (0.045f * elevation),
                                bounds.getCentreX(), bounds.getY(),
                                t.panelBackground.darker (0.03f * elevation),
                                bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (face);
    g.fillPath (shape);

    // A one-pixel highlight along the top edge only. This is the cheapest
    // convincing depth cue there is and the one most often left out.
    g.setColour (t.textBright.withAlpha (0.06f * elevation));
    g.drawLine (bounds.getX() + AbcTrainTheme::Radius::panel, bounds.getY() + 0.5f,
                 bounds.getRight() - AbcTrainTheme::Radius::panel, bounds.getY() + 0.5f, 1.0f);

    g.setColour (t.outline.withAlpha (0.9f));
    g.strokePath (shape, juce::PathStrokeType (1.0f));
}

void AbcTrainLookAndFeel::paintRecessedWell (juce::Graphics& g, juce::Rectangle<float> bounds,
                                              float radius)
{
    const auto& t = current();

    // Dark at the top, lighter at the bottom - the exact inverse of the
    // card above, which is what makes one read as cut in and the other as
    // sitting on top.
    //
    // How *much* darker has to differ by mode. On the dark page an 18%
    // step is a subtle shading; on the light page the same step is a grey
    // slab, and the whole analysis area read as a wash. On paper the depth
    // comes almost entirely from the inner shadow along the lip, which is
    // how it works on real paper too.
    const auto light = t.mode == AbcTrainTheme::Mode::light;
    const auto topStep = light ? 0.05f : 0.18f;
    const auto bottomStep = light ? 0.02f : 0.04f;

    juce::ColourGradient inside (t.displayBackground.darker (topStep),
                                  bounds.getCentreX(), bounds.getY(),
                                  t.displayBackground.brighter (bottomStep),
                                  bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (inside);
    g.fillRoundedRectangle (bounds, radius);

    // An inner shadow along the top lip only, drawn as a clipped stroke so
    // it follows the corner radius instead of stopping at the corners.
    {
        juce::Graphics::ScopedSaveState clipped (g);

        juce::Path shape;
        shape.addRoundedRectangle (bounds, radius);
        g.reduceClipRegion (shape);

        for (int i = 0; i < 3; ++i)
        {
            g.setColour (t.shadow.withAlpha ((light ? 0.16f : 0.10f)
                                              * t.shadowStrength * (float) (3 - i) / 3.0f));
            g.drawLine (bounds.getX(), bounds.getY() + 0.5f + (float) i,
                         bounds.getRight(), bounds.getY() + 0.5f + (float) i, 1.0f);
        }
    }

    g.setColour (t.outline.withAlpha (0.7f));
    g.drawRoundedRectangle (bounds, radius, 1.0f);
}

void AbcTrainLookAndFeel::paintSectionHeading (juce::Graphics& g, juce::Rectangle<float> bounds,
                                                const juce::String& caption)
{
    if (caption.isEmpty())
        return;

    const auto& t = current();

    const auto captionArea = bounds.withHeight (14.0f).withTrimmedLeft (2.0f);
    drawTrackedText (g, caption.toUpperCase(), captionArea, captionFont(),
                     t.textDim.withAlpha (0.7f), 1.2f, juce::Justification::centredLeft);

    // The rule starts past the caption rather than under it, so the
    // heading reads as sitting *on* the line rather than being boxed by
    // it.
    const auto captionWidth = trackedTextWidth (caption.toUpperCase(), captionFont(), 1.2f);
    const auto lineY = captionArea.getCentreY();
    const auto lineStart = captionArea.getX() + captionWidth + 10.0f;

    if (lineStart < bounds.getRight())
    {
        g.setColour (t.divider.withAlpha (0.7f));
        g.drawLine (lineStart, lineY, bounds.getRight(), lineY, 1.0f);
    }
}

void AbcTrainLookAndFeel::paintDisplayWell (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto& t = current();

    g.setColour (t.displayBackground);
    g.fillRoundedRectangle (bounds, Radius::well);

    // Inner shadow along the top edge, faked with a short gradient strip:
    // the cue that says "recessed" rather than "raised".
    juce::ColourGradient inner (t.shadow.withAlpha (0.35f * t.shadowStrength), bounds.getX(), bounds.getY(),
                                 juce::Colours::transparentBlack, bounds.getX(), bounds.getY() + 10.0f, false);
    g.setGradientFill (inner);
    g.fillRoundedRectangle (bounds, Radius::well);

    // A hairline, not a frame.
    //
    // At 0.75 this was a drawn box around every display in the product,
    // and a box around everything is what made the interface read as a
    // grid of cells rather than as a place. The recess above already says
    // "this is inset"; the outline only has to keep the shape from
    // dissolving into the panel, and 0.28 does that. Compare any current
    // reference - Pro-Q's analyser has no border at all, and its grid sits
    // at a few per cent, because everything that is not the signal is
    // supposed to disappear.
    g.setColour (t.outline.withAlpha (0.28f));
    g.drawRoundedRectangle (bounds, Radius::well, 1.0f);
}

const juce::Image& AbcTrainLookAndFeel::noiseTexture()
{
    // Built once, tiled forever. 128x128 is large enough that the tiling
    // never reads as a repeat at these sizes, small enough to be trivial.
    static const juce::Image texture = []
    {
        constexpr int size = 128;
        juce::Image image (juce::Image::ARGB, size, size, true);
        juce::Random random (0x9e3779b9);   // fixed seed: identical grain every run

        juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                // Signed noise around mid-grey so the overlay both darkens
                // and lightens; a purely additive grain would slowly wash
                // the whole surface lighter.
                const auto value = (juce::uint8) random.nextInt (256);
                data.setPixelColour (x, y, juce::Colour (value, value, value, (juce::uint8) 255));
            }
        }
        return image;
    }();

    return texture;
}

void AbcTrainLookAndFeel::overlayTexture (juce::Graphics& g, juce::Rectangle<float> bounds, float strength)
{
    const auto& t = current();
    const auto alpha = 0.022f * strength * t.textureStrength;

    if (alpha <= 0.001f)
        return;

    juce::Graphics::ScopedSaveState saved (g);
    g.reduceClipRegion (bounds.toNearestInt());
    g.setTiledImageFill (noiseTexture(), 0, 0, alpha);
    g.fillRect (bounds);
}

float AbcTrainLookAndFeel::trackedTextWidth (const juce::String& text, const juce::Font& font, float trackingPx)
{
    if (text.isEmpty())
        return 0.0f;

    juce::GlyphArrangement arrangement;
    arrangement.addLineOfText (font, text, 0.0f, 0.0f);
    const auto glyphs = arrangement.getNumGlyphs();

    // JUCE 8 moved string measurement off Font onto GlyphArrangement.
    return juce::GlyphArrangement::getStringWidth (font, text)
               + trackingPx * (float) juce::jmax (0, glyphs - 1);
}

void AbcTrainLookAndFeel::drawTrackedText (juce::Graphics& g, const juce::String& text,
                                            juce::Rectangle<float> area, const juce::Font& font,
                                            juce::Colour colour, float trackingPx,
                                            juce::Justification justification)
{
    if (text.isEmpty())
        return;

    // JUCE has no tracking/letter-spacing control on Font or drawText, so
    // the only way to set type wider is to lay the glyphs out and shift
    // each one by hand. GlyphArrangement gives the natural positions (with
    // the font's own kerning already applied); each glyph then gets an
    // extra cumulative offset and is drawn through its own transform.
    juce::GlyphArrangement arrangement;
    arrangement.addLineOfText (font, text, 0.0f, 0.0f);

    const auto numGlyphs = arrangement.getNumGlyphs();
    const auto totalWidth = trackedTextWidth (text, font, trackingPx);

    auto startX = area.getX();
    if (justification.testFlags (juce::Justification::horizontallyCentred))
        startX = area.getCentreX() - totalWidth * 0.5f;
    else if (justification.testFlags (juce::Justification::right))
        startX = area.getRight() - totalWidth;

    // GlyphArrangement lays out on a baseline at y=0, so shifting by the
    // wanted baseline puts the line where we asked for it.
    const auto baselineY = area.getCentreY() + (font.getAscent() - font.getDescent()) * 0.5f;

    g.setColour (colour);

    for (int i = 0; i < numGlyphs; ++i)
    {
        const auto& glyph = arrangement.getGlyph (i);
        if (glyph.isWhitespace())
            continue;

        glyph.draw (g, juce::AffineTransform::translation (startX + trackingPx * (float) i, baselineY));
    }
}

juce::Image AbcTrainLookAndFeel::blurredSnapshot (juce::Component& sourceComponent,
                                                   juce::Rectangle<int> sourceArea, float blurRadius)
{
    if (sourceArea.isEmpty())
        return {};

    // A genuine Gaussian blur, not a fake: render whatever is behind this
    // area into an offscreen image and convolve it.
    // juce::ImageConvolutionKernel is the one real blur primitive JUCE
    // ships, and this is the one place in the UI where the
    // layered-translucency trick used elsewhere wouldn't do - a tooltip
    // needs the content behind it genuinely diffused, not tinted.
    auto snapshot = sourceComponent.createComponentSnapshot (sourceArea, false);
    if (! snapshot.isValid())
        return {};

    juce::ImageConvolutionKernel kernel ((int) juce::jmax (3.0f, blurRadius));
    kernel.createGaussianBlur (blurRadius);
    kernel.applyToImage (snapshot, snapshot, snapshot.getBounds());
    return snapshot;
}

void AbcTrainLookAndFeel::paintBlurredBackdrop (juce::Graphics& g, juce::Component& sourceComponent,
                                                 juce::Rectangle<int> bounds, float blurRadius,
                                                 float cornerRadius)
{
    const auto snapshot = blurredSnapshot (sourceComponent, bounds, blurRadius);
    if (! snapshot.isValid())
        return;

    juce::Path clip;
    clip.addRoundedRectangle (bounds.toFloat(), cornerRadius);

    juce::Graphics::ScopedSaveState saved (g);
    g.reduceClipRegion (clip);
    g.drawImageAt (snapshot, bounds.getX(), bounds.getY());
}

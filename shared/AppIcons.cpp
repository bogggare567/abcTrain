#include "AppIcons.h"
#include "AbcTrainLookAndFeel.h"
#include "AbcTrainTheme.h"

namespace AppIcons
{
    namespace
    {
        juce::Path eqPath()
        {
            juce::Path p;
            p.startNewSubPath (5.0f, 3.0f); p.lineTo (5.0f, 21.0f);
            p.addEllipse (3.0f, 7.0f, 4.0f, 4.0f);
            p.startNewSubPath (12.0f, 3.0f); p.lineTo (12.0f, 21.0f);
            p.addEllipse (10.0f, 13.0f, 4.0f, 4.0f);
            p.startNewSubPath (19.0f, 3.0f); p.lineTo (19.0f, 21.0f);
            p.addEllipse (17.0f, 4.0f, 4.0f, 4.0f);
            return p;
        }

        juce::Path compressionPath()
        {
            juce::Path p;
            p.startNewSubPath (6.0f, 3.0f); p.lineTo (12.0f, 9.0f); p.lineTo (18.0f, 3.0f);
            p.startNewSubPath (6.0f, 21.0f); p.lineTo (12.0f, 15.0f); p.lineTo (18.0f, 21.0f);
            p.startNewSubPath (4.0f, 12.0f); p.lineTo (20.0f, 12.0f);
            return p;
        }

        juce::Path reverbPath()
        {
            juce::Path p;
            const juce::Point<float> origin (3.0f, 21.0f);
            for (float radius : { 6.0f, 11.0f, 16.0f })
                p.addCentredArc (origin.x, origin.y, radius, radius,
                                  0.0f, juce::MathConstants<float>::pi * 1.5f, juce::MathConstants<float>::twoPi, true);
            p.addEllipse (origin.x - 1.2f, origin.y - 1.2f, 2.4f, 2.4f);
            return p;
        }

        juce::Path panPath()
        {
            // A pan knob, turned. The old double-headed arrow said "wide"
            // rather than "which side" - it was indistinguishable from the
            // stereo-width glyph at tile size.
            juce::Path p;
            p.addCentredArc (12.0f, 13.0f, 7.5f, 7.5f, 0.0f,
                              juce::MathConstants<float>::pi * -0.75f,
                              juce::MathConstants<float>::pi * 0.75f, true);

            // The pointer, off-centre to the right: a centred one would
            // read as "no panning at all".
            p.startNewSubPath (12.0f, 13.0f);
            p.lineTo (17.3f, 7.7f);

            // Ticks at the two extremes, so the arc reads as a range.
            p.startNewSubPath (3.0f, 19.0f);  p.lineTo (5.0f, 19.0f);
            p.startNewSubPath (19.0f, 19.0f); p.lineTo (21.0f, 19.0f);
            return p;
        }

        juce::Path delayPath()
        {
            juce::Path p;
            p.addEllipse (0.0f, 8.0f, 8.0f, 8.0f);
            p.addEllipse (10.0f, 9.0f, 6.0f, 6.0f);
            p.addEllipse (18.0f, 10.0f, 4.0f, 4.0f);
            return p;
        }

        juce::Path distortionPath()
        {
            // A wave whose peaks have been squared off against two
            // ceilings. The old zig-zag was just a jagged line - it read
            // as "noise" or "spiky", not as clipping.
            juce::Path p;

            // The clip lines are the point of the mark, so they are drawn
            // first and the wave visibly flattens onto them.
            p.startNewSubPath (2.0f, 7.0f);  p.lineTo (22.0f, 7.0f);
            p.startNewSubPath (2.0f, 17.0f); p.lineTo (22.0f, 17.0f);

            p.startNewSubPath (2.0f, 12.0f);
            p.lineTo (5.0f, 7.0f);
            p.lineTo (9.0f, 7.0f);            // flat top
            p.lineTo (12.0f, 12.0f);
            p.lineTo (15.0f, 17.0f);
            p.lineTo (19.0f, 17.0f);          // flat bottom
            p.lineTo (22.0f, 12.0f);
            return p;
        }

        juce::Path stereoWidthPath()
        {
            // A centre point with two speakers pushed apart, and arrows
            // showing the push. Two overlapping circles read as a Venn
            // diagram, which says "overlap" - the opposite of width.
            juce::Path p;
            p.addRectangle (2.0f, 8.0f, 4.0f, 8.0f);    // left speaker
            p.addRectangle (18.0f, 8.0f, 4.0f, 8.0f);   // right speaker

            p.startNewSubPath (12.0f, 12.0f);
            p.lineTo (8.5f, 12.0f);
            p.startNewSubPath (8.5f, 12.0f); p.lineTo (10.5f, 10.0f);
            p.startNewSubPath (8.5f, 12.0f); p.lineTo (10.5f, 14.0f);

            p.startNewSubPath (12.0f, 12.0f);
            p.lineTo (15.5f, 12.0f);
            p.startNewSubPath (15.5f, 12.0f); p.lineTo (13.5f, 10.0f);
            p.startNewSubPath (15.5f, 12.0f); p.lineTo (13.5f, 14.0f);
            return p;
        }

        juce::Path gainPath()
        {
            juce::Path p;
            p.startNewSubPath (12.0f, 2.0f); p.lineTo (12.0f, 22.0f);
            p.startNewSubPath (12.0f, 2.0f); p.lineTo (9.0f, 6.0f);
            p.startNewSubPath (12.0f, 2.0f); p.lineTo (15.0f, 6.0f);
            p.startNewSubPath (12.0f, 22.0f); p.lineTo (9.0f, 18.0f);
            p.startNewSubPath (12.0f, 22.0f); p.lineTo (15.0f, 18.0f);
            p.startNewSubPath (8.0f, 12.0f); p.lineTo (16.0f, 12.0f);
            return p;
        }

        juce::Path frequencyRangePath()
        {
            // Bars, with a bracket picking out one span of them - naming a
            // *range* is the exercise. The old version bracketed the
            // middle three but drew the bracket so far under the bars that
            // at tile size it read as an underline, not a selection.
            juce::Path p;
            const float xs[] = { 3.5f, 8.0f, 12.5f, 17.0f, 21.0f };
            const float heights[] = { 5.0f, 11.0f, 15.0f, 9.0f, 4.0f };

            for (int i = 0; i < 5; ++i)
                p.addRectangle (xs[i] - 1.4f, 17.0f - heights[i], 2.8f, heights[i]);

            // The bracket hugs the bars it selects.
            p.startNewSubPath (6.0f, 20.5f); p.lineTo (6.0f, 22.5f);
            p.lineTo (15.0f, 22.5f);
            p.lineTo (15.0f, 20.5f);
            return p;
        }

        juce::Path learnerEQPath()
        {
            juce::Path p;
            p.startNewSubPath (2.0f, 16.0f);
            p.cubicTo (7.0f, 16.0f, 8.0f, 4.0f, 12.0f, 4.0f);
            p.cubicTo (16.0f, 4.0f, 17.0f, 16.0f, 22.0f, 16.0f);
            return p;
        }

        juce::Path learnerCompPath()
        {
            juce::Path p;
            p.addRectangle (3.0f, 4.0f, 18.0f, 4.0f);
            p.addRectangle (3.0f, 16.0f, 12.0f, 4.0f);
            p.startNewSubPath (12.0f, 9.0f); p.lineTo (12.0f, 15.0f);
            p.startNewSubPath (9.0f, 12.0f); p.lineTo (12.0f, 15.0f);
            p.startNewSubPath (15.0f, 12.0f); p.lineTo (12.0f, 15.0f);
            return p;
        }

        juce::Path learnerVerbPath()
        {
            juce::Path p;
            for (float radius : { 3.0f, 7.0f, 11.0f })
                p.addEllipse (12.0f - radius, 12.0f - radius, radius * 2.0f, radius * 2.0f);
            return p;
        }
    }

        // ---- interface icons ----
        // Same 24x24 box and thin-line vocabulary as the game glyphs, so
        // a title row of these reads as one set rather than borrowed
        // symbols.

        juce::Path soundPath()
        {
            // A speaker with two waves - "which sounds you train on".
            juce::Path p;
            p.startNewSubPath (4.0f, 9.0f);
            p.lineTo (8.0f, 9.0f);
            p.lineTo (12.0f, 5.0f);
            p.lineTo (12.0f, 19.0f);
            p.lineTo (8.0f, 15.0f);
            p.lineTo (4.0f, 15.0f);
            p.closeSubPath();

            p.startNewSubPath (15.5f, 9.0f);
            p.quadraticTo (17.5f, 12.0f, 15.5f, 15.0f);
            p.startNewSubPath (18.5f, 6.5f);
            p.quadraticTo (21.8f, 12.0f, 18.5f, 17.5f);
            return p;
        }

        juce::Path downloadPath()
        {
            // Arrow into a tray - the universal "get the new version".
            juce::Path p;
            p.startNewSubPath (12.0f, 4.0f);
            p.lineTo (12.0f, 14.5f);
            p.startNewSubPath (7.5f, 10.0f);
            p.lineTo (12.0f, 14.8f);
            p.lineTo (16.5f, 10.0f);

            p.startNewSubPath (5.0f, 17.0f);
            p.lineTo (5.0f, 20.0f);
            p.lineTo (19.0f, 20.0f);
            p.lineTo (19.0f, 17.0f);
            return p;
        }

        juce::Path sunPath()
        {
            juce::Path p;
            p.addEllipse (8.5f, 8.5f, 7.0f, 7.0f);

            for (int i = 0; i < 8; ++i)
            {
                const auto angle = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
                const auto centre = juce::Point<float> (12.0f, 12.0f);
                p.startNewSubPath (centre.getPointOnCircumference (8.8f, angle));
                p.lineTo (centre.getPointOnCircumference (11.0f, angle));
            }

            return p;
        }

        juce::Path moonPath()
        {
            // Crescent as one closed outline, so it strokes cleanly at
            // small sizes instead of showing two overlapping circles.
            juce::Path p;
            p.startNewSubPath (15.5f, 4.5f);
            p.quadraticTo (8.0f, 7.0f, 8.0f, 12.0f);
            p.quadraticTo (8.0f, 17.0f, 15.5f, 19.5f);
            p.quadraticTo (10.0f, 16.5f, 10.0f, 12.0f);
            p.quadraticTo (10.0f, 7.5f, 15.5f, 4.5f);
            p.closeSubPath();
            return p;
        }

        juce::Path homePath()
        {
            juce::Path p;
            p.startNewSubPath (4.0f, 11.5f);
            p.lineTo (12.0f, 4.5f);
            p.lineTo (20.0f, 11.5f);
            p.startNewSubPath (6.5f, 10.0f);
            p.lineTo (6.5f, 19.5f);
            p.lineTo (17.5f, 19.5f);
            p.lineTo (17.5f, 10.0f);
            return p;
        }

        juce::Path settingsPath()
        {
            // A gear: a ring with six teeth. Drawn from an arc rather than
            // an ellipse so the stroke weight matches every other glyph.
            juce::Path p;
            p.addEllipse (7.5f, 7.5f, 9.0f, 9.0f);

            for (int i = 0; i < 6; ++i)
            {
                const auto angle = juce::MathConstants<float>::twoPi * (float) i / 6.0f;
                const auto inner = juce::Point<float> (12.0f, 12.0f).getPointOnCircumference (7.0f, angle);
                const auto outer = juce::Point<float> (12.0f, 12.0f).getPointOnCircumference (10.5f, angle);

                p.startNewSubPath (inner);
                p.lineTo (outer);
            }

            return p;
        }

        juce::Path awardPath()
        {
            juce::Path p;
            p.addStar ({ 12.0f, 12.0f }, 5, 4.4f, 9.5f);
            return p;
        }

        juce::Path scopePath()
        {
            // A circle with a tilted trace - literally what the hint shows.
            juce::Path p;
            p.addEllipse (4.0f, 4.0f, 16.0f, 16.0f);
            p.startNewSubPath (9.0f, 16.5f);
            p.lineTo (15.0f, 7.5f);
            return p;
        }
    juce::Path getPath (Icon icon)
    {
        switch (icon)
        {
            case Icon::eq:             return eqPath();
            case Icon::compression:    return compressionPath();
            // Concentric rings, the same mark LearnerVerb uses. The old
            // corner ripple read as a wifi symbol, not as a space - which
            // is what it was supposed to say.
            case Icon::reverb:         return learnerVerbPath();
            case Icon::pan:            return panPath();
            case Icon::delay:          return delayPath();
            case Icon::distortion:     return distortionPath();
            case Icon::stereoWidth:    return stereoWidthPath();
            case Icon::gain:           return gainPath();
            case Icon::frequencyRange: return frequencyRangePath();
            case Icon::learnerEQ:      return learnerEQPath();
            case Icon::learnerComp:    return learnerCompPath();
            case Icon::learnerVerb:    return learnerVerbPath();
            case Icon::sound:          return soundPath();
            case Icon::download:       return downloadPath();
            case Icon::sun:            return sunPath();
            case Icon::moon:           return moonPath();
            case Icon::home:           return homePath();
            case Icon::scope:          return scopePath();
            case Icon::award:          return awardPath();
            case Icon::settings:       return settingsPath();
        }

        return eqPath();
    }

    Icon iconForGameName (const juce::String& englishName)
    {
        if (englishName == "Guess the Band")            return Icon::eq;
        if (englishName == "Guess the Compression")     return Icon::compression;
        if (englishName == "Guess the Reverb")          return Icon::reverb;
        if (englishName == "Guess the Pan Position")    return Icon::pan;
        if (englishName == "Guess the Delay Time")      return Icon::delay;
        if (englishName == "Guess the Distortion")      return Icon::distortion;
        if (englishName == "Guess the Stereo Width")    return Icon::stereoWidth;
        if (englishName == "Guess the Gain Change")     return Icon::gain;
        if (englishName == "Name the Range")            return Icon::frequencyRange;

        return Icon::eq; // an as-yet-unmapped game (see translateGameName's identical fallback shape)
    }

    void draw (juce::Graphics& g, Icon icon, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        auto path = getPath (icon);
        path.scaleToFit (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), true);

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawBadged (juce::Graphics& g, Icon icon, juce::Rectangle<float> bounds,
                     juce::Colour colour, float strength)
    {
        // The same glyph on a quiet plate.
        //
        // This started out tinting each exercise its family colour, which
        // turned the catalogue into nine saturated squares and made the
        // screen louder than anything on it. The plate is now neutral and
        // the glyph carries the weight; `colour` still comes through, but
        // heavily desaturated, so a family is a hint rather than a
        // declaration. Colour is reserved for the one place it means
        // something on its own - the achievement tiers.
        strength = juce::jlimit (0.0f, 1.0f, strength);

        const auto& theme = AbcTrainTheme::current();
        const auto lightMode = theme.mode == AbcTrainTheme::Mode::light;

        const auto plate = bounds.reduced (0.5f);
        const auto radius = plate.getWidth() * 0.28f;

        // The plate is a step away from the surface it sits on, in the
        // direction that surface has room to move - *down* on a light page,
        // *up* on a dark one. Building it out of an alpha of the tint (the
        // first version) meant it all but vanished in light mode, where a
        // 24% wash of anything over near-white is nothing at all.
        const auto plateColour = lightMode ? theme.windowBackground.darker (0.07f)
                                           : theme.widgetBackground.brighter (0.06f);

        juce::ColourGradient fill (plateColour.brighter (lightMode ? 0.02f : 0.05f),
                                    plate.getX(), plate.getY(),
                                    plateColour, plate.getX(), plate.getBottom(), false);
        g.setGradientFill (fill);
        g.setOpacity (0.55f + 0.45f * strength);
        g.fillRoundedRectangle (plate, radius);
        g.setOpacity (1.0f);

        g.setColour (theme.outline.withAlpha (0.4f + 0.4f * strength));
        g.drawRoundedRectangle (plate, radius, 1.0f);

        auto path = getPath (icon);
        const auto inner = plate.reduced (plate.getWidth() * 0.22f);
        path.scaleToFit (inner.getX(), inner.getY(), inner.getWidth(), inner.getHeight(), true);

        // The glyph carries the contrast, so it is drawn at close to full
        // strength even when the plate behind it is faded for a locked
        // achievement - a shape you cannot make out is not a hint, it is a
        // blank.
        g.setColour (colour.withAlpha (0.35f + 0.65f * strength));
        g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }
}

// ============================== IconButton ==============================

namespace
{
    constexpr int iconButtonTickHz = 60;
    constexpr double iconMorphMs = 220.0;
}

IconButton::IconButton (AppIcons::Icon initialIcon)
    : juce::Button ({}), icon (initialIcon), previousIcon (initialIcon)
{
    startTimerHz (iconButtonTickHz);
}

IconButton::~IconButton()
{
    stopTimer();
}

void IconButton::setIcon (AppIcons::Icon newIcon)
{
    if (newIcon == icon)
        return;

    previousIcon = icon;
    icon = newIcon;
    morph = 0.0f;
    repaint();
}

void IconButton::timerCallback()
{
    if (morph >= 1.0f)
        return;

    morph = juce::jmin (1.0f, morph + (float) (1000.0 / (double) iconButtonTickHz / iconMorphMs));
    repaint();
}

namespace AppIcons
{
    float hoverSpinDegrees (Icon icon) noexcept
    {
        switch (icon)
        {
            case Icon::settings:  return 32.0f;  // a gear turns
            case Icon::award:     return 72.0f;  // one point of a five-pointed star
            case Icon::reverb:
            case Icon::learnerVerb: return 18.0f;
            case Icon::pan:       return 12.0f;
            default:              return 0.0f;
        }
    }
}

void IconButton::paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown)
{
    const auto& theme = AbcTrainTheme::current();

    // Borrow the editor's own registry so an icon button eases exactly
    // like every text button beside it, rather than inventing a second
    // timing that would be almost-but-not-quite the same.
    float hover = shouldDrawButtonAsHighlighted ? 1.0f : 0.0f;
    float press = shouldDrawButtonAsDown ? 1.0f : 0.0f;

    if (auto* laf = dynamic_cast<AbcTrainLookAndFeel*> (&getLookAndFeel()))
    {
        hover = AbcTrainTheme::Ease::out (laf->getStateRegistry().hoverAmount (*this, shouldDrawButtonAsHighlighted));
        press = AbcTrainTheme::Ease::out (laf->getStateRegistry().pressAmount (*this, shouldDrawButtonAsDown));
    }

    const auto bounds = getLocalBounds().toFloat().reduced (1.0f).translated (0.0f, press);

    // No frame at rest: a title row of six outlined boxes is exactly the
    // noise this replaced. The surface only appears under the pointer.
    if (hover > 0.01f)
    {
        g.setColour (theme.widgetBackground.withAlpha (0.75f * hover));
        g.fillRoundedRectangle (bounds, AbcTrainTheme::Radius::button);
        g.setColour (theme.outline.withAlpha (0.7f * hover));
        g.drawRoundedRectangle (bounds, AbcTrainTheme::Radius::button, 1.0f);
    }

    auto iconArea = bounds.reduced (bounds.getWidth() * 0.26f);

    // Press squashes the glyph and the release springs it back past its
    // own size before settling - backOut, not out. A control that returns
    // to rest along the same curve it left by has no mass; the overshoot
    // is the entire difference between "it moved" and "I pushed it".
    const auto squash = 1.0f - 0.10f * press + 0.03f * AbcTrainTheme::Ease::backOut (1.0f - press) * press;
    iconArea = iconArea.withSizeKeepingCentre (iconArea.getWidth() * squash,
                                                iconArea.getHeight() * squash);

    const auto spin = AppIcons::hoverSpinDegrees (icon) * hover;

    juce::Graphics::ScopedSaveState rotated (g);

    if (std::abs (spin) > 0.01f)
        g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (spin),
                                                          iconArea.getCentreX(),
                                                          iconArea.getCentreY()));
    const auto tint = theme.text.interpolatedWith (theme.textBright, hover)
                          .withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f);

    // Cross-fade between glyphs. Both are drawn during the transition,
    // the outgoing one shrinking slightly as it fades, so a state change
    // reads as a morph rather than a swap.
    const auto eased = AbcTrainTheme::Ease::out (morph);

    if (eased < 1.0f)
    {
        juce::Graphics::ScopedSaveState saved (g);
        g.setOpacity (1.0f - eased);
        AppIcons::draw (g, previousIcon, iconArea.reduced (iconArea.getWidth() * 0.12f * eased),
                        tint.withMultipliedAlpha (1.0f - eased));
    }

    {
        juce::Graphics::ScopedSaveState saved (g);
        g.setOpacity (eased);
        AppIcons::draw (g, icon, iconArea.reduced (iconArea.getWidth() * 0.12f * (1.0f - eased)),
                        tint.withMultipliedAlpha (eased));
    }
}

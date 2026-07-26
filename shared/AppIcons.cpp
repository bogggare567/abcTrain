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
            juce::Path p;
            p.startNewSubPath (3.0f, 12.0f); p.lineTo (21.0f, 12.0f);
            p.startNewSubPath (3.0f, 12.0f); p.lineTo (6.0f, 9.0f);
            p.startNewSubPath (3.0f, 12.0f); p.lineTo (6.0f, 15.0f);
            p.startNewSubPath (21.0f, 12.0f); p.lineTo (18.0f, 9.0f);
            p.startNewSubPath (21.0f, 12.0f); p.lineTo (18.0f, 15.0f);
            p.addEllipse (12.5f, 9.5f, 5.0f, 5.0f);
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
            juce::Path p;
            p.startNewSubPath (2.0f, 16.0f);
            p.lineTo (5.0f, 16.0f); p.lineTo (7.0f, 6.0f); p.lineTo (9.0f, 6.0f);
            p.lineTo (11.0f, 18.0f); p.lineTo (13.0f, 18.0f);
            p.lineTo (15.0f, 4.0f); p.lineTo (17.0f, 4.0f);
            p.lineTo (19.0f, 14.0f); p.lineTo (22.0f, 14.0f);
            return p;
        }

        juce::Path stereoWidthPath()
        {
            juce::Path p;
            p.addEllipse (2.0f, 6.0f, 13.0f, 13.0f);
            p.addEllipse (9.0f, 6.0f, 13.0f, 13.0f);
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
            juce::Path p;
            const float xs[] = { 3.0f, 7.0f, 11.0f, 15.0f, 19.0f };
            const float heights[] = { 8.0f, 14.0f, 18.0f, 10.0f, 6.0f };
            for (int i = 0; i < 5; ++i)
                p.addRectangle (xs[i] - 1.5f, 18.0f - heights[i], 3.0f, heights[i]);
            p.startNewSubPath (5.5f, 20.0f); p.lineTo (5.5f, 22.0f);
            p.startNewSubPath (5.5f, 22.0f); p.lineTo (16.5f, 22.0f);
            p.startNewSubPath (16.5f, 22.0f); p.lineTo (16.5f, 20.0f);
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
            case Icon::reverb:         return reverbPath();
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

    const auto iconArea = bounds.reduced (bounds.getWidth() * 0.26f);
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

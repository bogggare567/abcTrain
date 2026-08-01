#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AbcTrainTheme.h"
#include <array>
#include <cmath>

// The welcome screen's background: the instruments this product is about,
// drifting.
//
// It replaces three letters hopping up and down. That animation was the
// only thing moving on the screen, which put all the weight of "this is
// alive" on a gag - and a gag that reads as childish next to the rest of
// the product. The letters are still now, in the icon's own colours; the
// motion moved to the background, where it belongs.
//
// Four instruments cycle: an EQ curve breathing over its own band, a bank
// of faders settling, a vectorscope tracing a Lissajous figure, and a
// spectrum. Each one holds for a few seconds, then cross-fades to the
// next, so the screen is never still and never busy.
//
// **It is drawn, not measured.** No audio is playing on this screen, and a
// meter reading nothing would be a lie about what the product does; these
// are figures moving to their own slow clocks. They sit far back - a few
// per cent of alpha - because a background that competes with the wordmark
// is not a background.
class AmbientInstruments
{
public:
    // `phase` counts seconds; the caller owns the clock, so a screen that
    // stops animating simply stops advancing it.
    static void paint (juce::Graphics& g, juce::Rectangle<float> bounds, double phase)
    {
        if (bounds.isEmpty())
            return;

        const auto& theme = AbcTrainTheme::current();

        // Four scenes, held then dissolved into each other. One overlap
        // per boundary and nothing else: the first version also ramped a
        // scene up from zero at its *own* start, which meant the incoming
        // figure arrived at full strength through the overlap and then
        // snapped to black to fade in a second time - a visible dip at
        // every boundary, and exactly the "ugly transition" it read as.
        //
        // The ramp is smoothstepped. A linear alpha ramp spends half its
        // time looking finished; the eased one moves slowest exactly at
        // the ends, which is what a dissolve is.
        constexpr double hold = 8.0;
        constexpr double fade = 2.6;

        const auto cycle = std::fmod (phase, hold * 4.0);
        const auto scene = (int) (cycle / hold);
        const auto within = cycle - scene * hold;

        const auto linear = within > hold - fade ? (float) ((hold - within) / fade) : 1.0f;
        const auto strength = linear * linear * (3.0f - 2.0f * linear);
        const auto incoming = 1.0f - strength;

        const auto next = (scene + 1) % 4;

        juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (bounds.toNearestInt());

        drawScene (g, bounds, scene, phase, strength, theme);

        if (incoming > 0.001f)
            drawScene (g, bounds, next, phase, incoming, theme);
    }

private:
    static juce::Colour hue (int scene, const AbcTrainTheme::Palette& theme)
    {
        switch (scene)
        {
            case 0:  return AbcTrainTheme::accentFor (AbcTrainTheme::Family::frequency);
            case 1:  return AbcTrainTheme::accentFor (AbcTrainTheme::Family::dynamics);
            case 2:  return AbcTrainTheme::accentFor (AbcTrainTheme::Family::space);
            default: return AbcTrainTheme::accentFor (AbcTrainTheme::Family::character);
        }

        return theme.accent;
    }

    static void drawScene (juce::Graphics& g, juce::Rectangle<float> bounds, int scene,
                            double phase, float strength, const AbcTrainTheme::Palette& theme)
    {
        // Visible, but still behind. The first attempt sat at 9% alpha on
        // a near-black page, which is indistinguishable from an empty
        // screen - the exact complaint it was built to answer. These are
        // wallpaper, not readouts; the test is that you notice the screen
        // is alive without ever reading a value off it.
        const auto colour = hue (scene, theme).withAlpha (0.30f * strength);
        const auto faint = hue (scene, theme).withAlpha (0.13f * strength);

        switch (scene)
        {
            case 0:  drawEqCurve (g, bounds, phase, colour, faint); break;
            case 1:  drawFaders  (g, bounds, phase, colour, faint); break;
            case 2:  drawScope   (g, bounds, phase, colour);        break;
            default: drawSpectrum (g, bounds, phase, colour, faint); break;
        }
    }

    // A bell moving slowly across the spectrum, the way somebody sweeps
    // for a resonance. Sized to the window, not to a safe little middle:
    // this is the backdrop, and a backdrop that stops short of the edges
    // reads as a widget.
    static void drawEqCurve (juce::Graphics& g, juce::Rectangle<float> b, double phase,
                             juce::Colour colour, juce::Colour faint)
    {
        const auto centre = 0.35f + 0.30f * (float) std::sin (phase * 0.31);
        const auto width = 0.12f + 0.05f * (float) std::sin (phase * 0.19);
        const auto gain = 0.40f * (float) std::sin (phase * 0.23);

        juce::Path path;
        constexpr int points = 160;

        for (int i = 0; i < points; ++i)
        {
            const auto t = (float) i / (points - 1);
            const auto d = (t - centre) / width;
            const auto bell = gain * std::exp (-d * d);
            const auto x = b.getX() + b.getWidth() * t;
            const auto y = b.getCentreY() - b.getHeight() * bell;

            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }

        g.setColour (faint);
        g.drawLine (b.getX(), b.getCentreY(), b.getRight(), b.getCentreY(), 1.0f);

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved));
    }

    // A bank of faders settling at different rates - what a mix looks like
    // while somebody is still deciding.
    static void drawFaders (juce::Graphics& g, juce::Rectangle<float> b, double phase,
                            juce::Colour colour, juce::Colour faint)
    {
        constexpr int count = 12;
        const auto slot = b.getWidth() / count;
        const auto top = b.getY() + b.getHeight() * 0.06f;
        const auto bottom = b.getBottom() - b.getHeight() * 0.06f;

        // Wide enough to read as a fader rather than as a dash. The first
        // version drew a 10px cap on a barely-visible hairline, which at
        // full-window size scattered a dozen little rectangles across the
        // screen with nothing joining them - debris, not a console.
        const auto capWidth = slot * 0.52f;
        const auto capHeight = juce::jlimit (8.0f, 16.0f, b.getHeight() * 0.018f);
        const auto trackWidth = juce::jmax (3.0f, slot * 0.10f);

        for (int i = 0; i < count; ++i)
        {
            // Each fader has its own speed and offset, so the bank never
            // moves as one block - which is what would make it read as a
            // graphic rather than as faders.
            const auto speed = 0.16 + 0.05 * (i % 5);
            const auto value = 0.5 + 0.38 * std::sin (phase * speed + i * 1.7);

            const auto x = b.getX() + slot * (i + 0.5f);
            const auto y = (float) (bottom - (bottom - top) * value);

            // Track, then the travelled part of it, then the cap. The
            // filled portion is what ties a cap to its slot: without it
            // the eye has no reason to connect the two, which is why the
            // bank read as floating marks.
            g.setColour (faint);
            g.fillRoundedRectangle (x - trackWidth * 0.5f, top, trackWidth, bottom - top,
                                     trackWidth * 0.5f);

            g.setColour (colour.withMultipliedAlpha (0.55f));
            g.fillRoundedRectangle (x - trackWidth * 0.5f, y, trackWidth, bottom - y,
                                     trackWidth * 0.5f);

            g.setColour (colour);
            g.fillRoundedRectangle (x - capWidth * 0.5f, y - capHeight * 0.5f,
                                     capWidth, capHeight, capHeight * 0.35f);
        }
    }

    // A Lissajous figure - the vectorscope's own picture of a stereo
    // field, wandering between narrow and wide.
    static void drawScope (juce::Graphics& g, juce::Rectangle<float> b, double phase,
                           juce::Colour colour)
    {
        const auto radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.42f;
        const auto centre = b.getCentre();
        const auto width = 0.35f + 0.5f * (float) (0.5 + 0.5 * std::sin (phase * 0.21));

        juce::Path path;
        constexpr int points = 420;

        for (int i = 0; i < points; ++i)
        {
            const auto t = (double) i / points * juce::MathConstants<double>::twoPi * 3.0;
            const auto l = std::sin (t * 2.0 + phase * 0.6);
            const auto r = std::sin (t * 3.0 + phase * 0.37) * width;

            // Mid/side rotation: the same 45-degree view a real
            // vectorscope shows, so mono collapses to a vertical line.
            const auto x = centre.x + (float) ((l - r) * radius * 0.707);
            const auto y = centre.y - (float) ((l + r) * radius * 0.707);

            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.6f));
    }

    // A spectrum as a **curve**, not a row of bars.
    //
    // Bars were the first version and they read as a cheap equaliser
    // graphic - a wall of rectangles is the tell of a fake spectrum, and
    // it is not what a real analyser looks like either once any smoothing
    // is on. This is one flowing shape, filled under the line, with the
    // envelope sampled at far more points than it has peaks so the curve
    // itself stays smooth however wide the window gets.
    static void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> b, double phase,
                              juce::Colour colour, juce::Colour faint)
    {
        const auto floorY = b.getBottom() - b.getHeight() * 0.06f;
        const auto span = b.getHeight() * 0.72f;

        constexpr int points = 220;

        juce::Path curve;
        curve.startNewSubPath (b.getX(), floorY);

        for (int i = 0; i < points; ++i)
        {
            const auto t = (float) i / (points - 1);

            // Tilted down towards the top end, which is what pink-ish
            // material actually looks like - a level shelf across the
            // whole width is the other tell of a fake spectrum.
            const auto tilt = std::pow (1.0f - t, 0.7f);

            // Three slow ripples of different rate and wavelength summed
            // into one envelope. A single sine would read as a wave; three
            // incommensurate ones never repeat a shape long enough to look
            // periodic, which is what makes it read as material.
            const auto ripple = 0.45 * std::sin (phase * 0.55 + t * 11.0)
                              + 0.32 * std::sin (phase * 0.37 - t * 19.0 + 1.7)
                              + 0.23 * std::sin (phase * 0.81 + t * 6.0 + 3.1);

            const auto height = span * tilt * (float) (0.30 + 0.42 * (0.5 + 0.5 * ripple));

            curve.lineTo (b.getX() + b.getWidth() * t, floorY - height);
        }

        curve.lineTo (b.getRight(), floorY);
        curve.closeSubPath();

        // Filled body plus a brighter edge: the fill is what makes it a
        // spectrum rather than a line drawing, the edge is what keeps it
        // legible where the fill is thin.
        g.setColour (faint);
        g.fillPath (curve);

        g.setColour (colour);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
    }
};

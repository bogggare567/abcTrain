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

        // Four scenes, ~7s each, with a 1.4s cross-fade between them so
        // nothing ever pops.
        constexpr double hold = 7.0;
        constexpr double fade = 1.4;

        const auto cycle = std::fmod (phase, hold * 4.0);
        const auto scene = (int) (cycle / hold);
        const auto within = cycle - scene * hold;

        auto strength = 1.0f;
        if (within < fade)              strength = (float) (within / fade);
        else if (within > hold - fade)  strength = (float) ((hold - within) / fade);

        const auto next = (scene + 1) % 4;
        const auto incoming = within > hold - fade ? 1.0f - strength : 0.0f;

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
    // for a resonance.
    static void drawEqCurve (juce::Graphics& g, juce::Rectangle<float> b, double phase,
                             juce::Colour colour, juce::Colour faint)
    {
        const auto centre = 0.35f + 0.30f * (float) std::sin (phase * 0.31);
        const auto width = 0.12f + 0.05f * (float) std::sin (phase * 0.19);
        const auto gain = 0.28f * (float) std::sin (phase * 0.23);

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

        for (int i = 0; i < count; ++i)
        {
            // Each fader has its own speed and offset, so the bank never
            // moves as one block - which is what would make it read as a
            // graphic rather than as faders.
            const auto speed = 0.16 + 0.05 * (i % 5);
            const auto value = 0.5 + 0.34 * std::sin (phase * speed + i * 1.7);

            const auto x = b.getX() + slot * (i + 0.5f);
            const auto top = b.getY() + b.getHeight() * 0.18f;
            const auto bottom = b.getBottom() - b.getHeight() * 0.18f;
            const auto y = (float) (bottom - (bottom - top) * value);

            g.setColour (faint);
            g.drawLine (x, top, x, bottom, 1.0f);

            g.setColour (colour);
            g.fillRoundedRectangle (x - slot * 0.22f, y - 5.0f, slot * 0.44f, 10.0f, 3.0f);
        }
    }

    // A Lissajous figure - the vectorscope's own picture of a stereo
    // field, wandering between narrow and wide.
    static void drawScope (juce::Graphics& g, juce::Rectangle<float> b, double phase,
                           juce::Colour colour)
    {
        const auto radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.28f;
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

    // Bars falling at different rates, as a spectrum does.
    static void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> b, double phase,
                              juce::Colour colour, juce::Colour faint)
    {
        constexpr int bars = 34;
        const auto slot = b.getWidth() / bars;
        const auto floorY = b.getBottom() - b.getHeight() * 0.2f;
        const auto span = b.getHeight() * 0.5f;

        for (int i = 0; i < bars; ++i)
        {
            const auto t = (float) i / bars;

            // Tilted down towards the top end, which is what pink-ish
            // material actually looks like - a flat wall of bars is the
            // tell of a fake spectrum.
            const auto tilt = std::pow (1.0f - t, 0.7f);
            const auto wobble = 0.5 + 0.5 * std::sin (phase * (0.7 + t * 1.6) + i * 0.9);
            const auto height = span * tilt * (float) (0.25 + 0.75 * wobble);

            const auto x = b.getX() + slot * i;

            g.setColour (i % 2 == 0 ? colour : faint);
            g.fillRect (x + slot * 0.15f, floorY - height, slot * 0.7f, height);
        }
    }
};

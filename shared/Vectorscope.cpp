#include "Vectorscope.h"
#include "AbcTrainLookAndFeel.h"
#include "AbcTrainTheme.h"
#include <cmath>

namespace
{
    constexpr float invSqrt2 = 0.70710678f;
}

Vectorscope::Vectorscope()
{
    setOpaque (true);
    reset();
    startTimerHz (30);
}

Vectorscope::~Vectorscope()
{
    stopTimer();
}

void Vectorscope::reset() noexcept
{
    for (int i = 0; i < numPoints; ++i)
    {
        pointsX[(size_t) i].store (0.0f, std::memory_order_relaxed);
        pointsY[(size_t) i].store (0.0f, std::memory_order_relaxed);
    }

    writeIndex.store (0, std::memory_order_relaxed);
}

void Vectorscope::pushSample (float left, float right) noexcept
{
    // The 45-degree rotation: mid on the vertical axis, side on the
    // horizontal one. Without it a mono signal draws a diagonal, which is
    // not what anyone expects a goniometer to look like.
    const auto x = (right - left) * invSqrt2;
    const auto y = (right + left) * invSqrt2;

    const auto index = writeIndex.load (std::memory_order_relaxed);
    pointsX[(size_t) index].store (x, std::memory_order_relaxed);
    pointsY[(size_t) index].store (y, std::memory_order_relaxed);
    writeIndex.store ((index + 1) % numPoints, std::memory_order_relaxed);
}

void Vectorscope::timerCallback()
{
    repaint();
}

void Vectorscope::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto bounds = getLocalBounds().toFloat();

    g.fillAll (theme.displayBackground);
    AbcTrainLookAndFeel::overlayTexture (g, bounds, 0.6f);

    const auto centre = bounds.getCentre();
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.42f;

    // ---- graticule ----
    g.setColour (theme.textDim.withAlpha (0.13f));
    g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.0f);
    g.drawLine (centre.x, centre.y - radius, centre.x, centre.y + radius, 1.0f);
    g.drawLine (centre.x - radius, centre.y, centre.x + radius, centre.y, 1.0f);

    // The two diagonals are where a hard-panned source lands, so they are
    // the reference the eye actually measures a tilt against.
    const auto diagonal = radius * invSqrt2;
    g.setColour (theme.textDim.withAlpha (0.08f));
    g.drawLine (centre.x - diagonal, centre.y - diagonal, centre.x + diagonal, centre.y + diagonal, 1.0f);
    g.drawLine (centre.x - diagonal, centre.y + diagonal, centre.x + diagonal, centre.y - diagonal, 1.0f);

    // Labels on all four references, not just the sides. Without saying
    // what the axes mean, a goniometer is an abstract shape - which is
    // exactly the complaint this addresses.
    g.setFont (AbcTrainLookAndFeel::captionFont());

    const auto label = [&] (const juce::String& text, juce::Point<float> at, juce::Colour colour)
    {
        g.setColour (colour);
        g.drawText (text, juce::Rectangle<float> (at.x - 16.0f, at.y - 8.0f, 32.0f, 16.0f),
                     juce::Justification::centred, false);
    };

    // All labels sit *inside* the circle. This scope is only ~74px in the
    // hint strip, so anything placed outside the radius is clipped by the
    // component - which is exactly what happened to "M" and "+/-S" on the
    // first attempt.
    label ("L", { centre.x - radius * 0.72f, centre.y - radius * 0.72f }, theme.textDim.withAlpha (0.55f));
    label ("R", { centre.x + radius * 0.72f, centre.y - radius * 0.72f }, theme.textDim.withAlpha (0.55f));

    // The vertical axis is mono - the one an ear-training player most
    // needs named, since "is this centred" is the question being asked.
    label ("M", { centre.x, centre.y - radius * 0.78f }, theme.accent.withAlpha (0.7f));

    // Running correlation, so "how mono is this" is a number as well as
    // a shape. +1 is mono, 0 is wide, -1 is fully out of phase.
    {
        double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;

        for (int i = 0; i < numPoints; ++i)
        {
            const auto x = pointsX[(size_t) i].load (std::memory_order_relaxed);
            const auto y = pointsY[(size_t) i].load (std::memory_order_relaxed);

            // Undo the 45-degree rotation to get the channels back.
            const auto l = (y - x) * invSqrt2;
            const auto r = (y + x) * invSqrt2;

            sumLR += (double) l * r;
            sumLL += (double) l * l;
            sumRR += (double) r * r;
        }

        const auto denominator = std::sqrt (sumLL * sumRR);
        const auto correlation = denominator > 1.0e-9 ? sumLR / denominator : 1.0;

        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (10.0f));
        g.setColour (theme.textDim.withAlpha (0.75f));
        g.drawText (juce::String (correlation, 2),
                     bounds.withTop (bounds.getBottom() - 14.0f).reduced (4.0f, 0.0f),
                     juce::Justification::centredLeft, false);
    }

    // ---- the trace ----
    // Drawn oldest-to-newest with rising alpha, so the trail reads as
    // motion rather than a static smear.
    const auto newest = writeIndex.load (std::memory_order_relaxed);

    for (int age = numPoints - 1; age >= 0; --age)
    {
        const auto index = ((newest - 1 - age) % numPoints + numPoints) % numPoints;

        const auto x = pointsX[(size_t) index].load (std::memory_order_relaxed);
        const auto y = pointsY[(size_t) index].load (std::memory_order_relaxed);

        if (std::abs (x) < 1.0e-5f && std::abs (y) < 1.0e-5f)
            continue;

        const auto freshness = 1.0f - (float) age / (float) numPoints;
        const auto alpha = 0.05f + 0.55f * freshness * freshness;

        // y is negated: the scope's positive axis points up, the screen's
        // points down.
        const auto px = centre.x + juce::jlimit (-1.2f, 1.2f, x) * radius;
        const auto py = centre.y - juce::jlimit (-1.2f, 1.2f, y) * radius;

        g.setColour (theme.accent.withAlpha (alpha));
        g.fillEllipse (px - 1.0f, py - 1.0f, 2.0f, 2.0f);
    }
}

#include "GainReductionMeter.h"
#include "AbcTrainLookAndFeel.h"
#include "AbcTrainTheme.h"
#include <cmath>

namespace
{
    // Same sweep as the rotary knobs above it, so the meter reads as part
    // of the same instrument rather than a borrowed widget.
    constexpr float arcStart = juce::MathConstants<float>::pi * 1.2f;
    constexpr float arcEnd   = juce::MathConstants<float>::pi * 2.8f;
}

GainReductionMeter::GainReductionMeter()
{
    startTimerHz (60);
}

GainReductionMeter::~GainReductionMeter()
{
    stopTimer();
}

void GainReductionMeter::timerCallback()
{
    const auto previous = displayedDb;

    // Asymmetric smoothing: catch the onset of reduction quickly, let it
    // fall back slowly. A symmetric filter either misses fast reduction or
    // leaves the needle twitching on release.
    const auto coefficient = targetDb > displayedDb ? 0.45f : 0.10f;
    displayedDb += (targetDb - displayedDb) * coefficient;

    const auto targetGlow = juce::jlimit (0.0f, 1.0f, displayedDb / (rangeDb * 0.5f));
    glow += (targetGlow - glow) * 0.2f;

    if (std::abs (displayedDb - previous) > 0.005f || std::abs (targetGlow - glow) > 0.005f)
        repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    auto bounds = getLocalBounds().toFloat().reduced (4.0f);
    // Reserve the lower strip for the numeric readout, then centre the arc
    // in what's left.
    const auto labelHeight = 16.0f;
    const auto arcArea = bounds.withTrimmedBottom (labelHeight);

    const auto radius = juce::jmin (arcArea.getWidth(), arcArea.getHeight() * 1.6f) * 0.42f;
    const auto centre = juce::Point<float> (arcArea.getCentreX(), arcArea.getBottom() - radius * 0.25f);
    constexpr float thickness = 7.0f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, arcStart, arcEnd, true);

    g.setColour (theme.displayBackground);
    g.strokePath (track, juce::PathStrokeType (thickness + 2.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    g.setColour (theme.outline);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    const auto proportion = juce::jlimit (0.0f, 1.0f, displayedDb / rangeDb);

    if (proportion > 0.001f)
    {
        const auto endAngle = arcStart + proportion * (arcEnd - arcStart);

        juce::Path fill;
        fill.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, arcStart, endAngle, true);

        // Glow first, underneath: progressively wider, fainter copies. The
        // whole point of this meter is that heavy compression should be
        // visible from the corner of your eye without reading a number.
        if (glow > 0.01f)
        {
            for (int layer = 3; layer >= 1; --layer)
            {
                g.setColour (theme.negative.withAlpha (0.13f * glow / (float) layer));
                g.strokePath (fill, juce::PathStrokeType (thickness + 3.0f * (float) layer,
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
            }
        }

        // Gradient along the sweep: calm accent where reduction is gentle,
        // warm then hot as it deepens. Because the gradient is anchored to
        // the arc's geometry rather than to the current value, a given
        // amount of reduction always lands on the same colour.
        juce::ColourGradient sweep (theme.accent, centre.x - radius, centre.y,
                                     theme.negative, centre.x + radius, centre.y, false);
        sweep.addColour (0.5, theme.accentWarm);
        g.setGradientFill (sweep);
        g.strokePath (fill, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Numeric readout in the monospaced face, so the digits don't jitter
    // horizontally as the value changes.
    g.setColour (displayedDb > 0.5f ? theme.textBright : theme.textDim);
    g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (13.0f));
    g.drawText (juce::String (displayedDb, 1) + " dB",
                bounds.removeFromBottom (labelHeight), juce::Justification::centred, false);

    g.setColour (theme.textDim.withAlpha (0.7f));
    g.setFont (AbcTrainLookAndFeel::captionFont());
    // The arrow is not decoration: this is the one meter where more is
    // lower, and a newcomer reads an unlabelled falling arc backwards.
    g.drawText (juce::String::fromUTF8 ("GR \xe2\x86\x93"), arcArea.withHeight (14.0f),
                 juce::Justification::centredTop, false);
}

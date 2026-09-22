#include "shared/analysis/GainReductionMeter.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include <cmath>


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
    // A bar that hangs from 0 dB at the right and grows leftwards, in
    // segments you can count (ADR 037). It was an arc with its readout
    // drawn inside the ring, and at the trainer's type sizes the number and
    // the ring ran into each other. Left-growing is what a hardware GR
    // meter's needle does, and "more is lower" is still said by the arrow.
    const auto& theme = AbcTrainTheme::current();
    auto area = getLocalBounds().toFloat().reduced (2.0f, 0.0f);

    const auto labelFont = AbcTrainLookAndFeel::labelFont();
    const auto caption = juce::String::fromUTF8 ("GR \xe2\x86\x93");
    const auto captionWidth = AbcTrainLookAndFeel::trackedTextWidth (caption, labelFont, 1.3f) + 12.0f;

    AbcTrainLookAndFeel::drawTrackedText (g, caption, area.removeFromLeft (captionWidth), labelFont,
                                          theme.textDim, 1.3f, juce::Justification::centredLeft);

    auto readout = area.removeFromRight (86.0f);
    g.setColour (displayedDb > 0.5f ? theme.textBright : theme.textDim);
    g.setFont (AbcTrainLookAndFeel::monoFont());
    g.drawText ((displayedDb > 0.05f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : juce::String())
                    + juce::String (displayedDb, 1).replace (".", decimal) + " " + unit,
                readout, juce::Justification::centredRight, false);

    area.removeFromRight (10.0f);

    auto block = area.withSizeKeepingCentre (area.getWidth(), 30.0f);
    auto bar = block.removeFromTop (14.0f);
    block.removeFromTop (2.0f);
    auto scale = block;

    constexpr int segments = 24;
    const auto gap = 2.0f;
    const auto w = (bar.getWidth() - gap * (segments - 1)) / (float) segments;
    const auto lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, displayedDb / rangeDb) * segments);

    for (int i = 0; i < segments; ++i)
    {
        // Segment 0 is the rightmost: 0 to 1 dB of reduction.
        const auto x = bar.getRight() - (float) (i + 1) * w - (float) i * gap;
        const auto seg = juce::Rectangle<float> (x, bar.getY(), w, bar.getHeight());

        if (i < lit)
        {
            const auto t = (float) i / (float) (segments - 1);
            g.setColour (t < 0.25f ? theme.accent : t < 0.5f ? theme.accentWarm : theme.negative);
        }
        else
        {
            g.setColour (theme.outline.withAlpha (0.7f));
        }

        g.fillRect (seg);
    }

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::microFont());

    for (int db : { 0, 6, 12, 18, 24 })
    {
        const auto x = bar.getRight() - bar.getWidth() * (float) db / rangeDb;
        g.drawText (db == 0 ? juce::String ("0") : juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + juce::String (db),
                    juce::Rectangle<float> (x - 20.0f, scale.getY(), 40.0f, scale.getHeight()),
                    juce::Justification::centred, false);
    }
}

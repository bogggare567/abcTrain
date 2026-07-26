#include "SpectrumAnalyzer.h"
#include "AbcTrainLookAndFeel.h"
#include "AbcTrainTheme.h"

SpectrumAnalyzerComponent::SpectrumAnalyzerComponent()
    : forwardFFT (fftOrder),
      window (fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    setOpaque (true);
    startTimerHz (30);
}

void SpectrumAnalyzerComponent::pushNextSampleIntoFifo (float sample) noexcept
{
    if (fifoIndex == fftSize)
    {
        if (! nextFFTBlockReady)
        {
            std::copy (fifo.begin(), fifo.end(), fftData.begin());
            nextFFTBlockReady = true;
        }
        fifoIndex = 0;
    }

    fifo[(size_t) fifoIndex++] = sample;
}

float SpectrumAnalyzerComponent::proportionToFrequency (float proportion) noexcept
{
    return (float) (minFreq * std::pow (maxFreq / minFreq, proportion));
}

float SpectrumAnalyzerComponent::frequencyToProportion (float frequency) noexcept
{
    return (float) (std::log (frequency / minFreq) / std::log (maxFreq / minFreq));
}

void SpectrumAnalyzerComponent::timerCallback()
{
    if (nextFFTBlockReady)
    {
        drawNextFrameOfSpectrum();
        nextFFTBlockReady = false;
    }

    // Ease the *displayed* curve toward the analysis result every frame,
    // whether or not a new FFT block arrived - otherwise the curve would
    // freeze between blocks and then jump.
    for (size_t i = 0; i < (size_t) scopeSize; ++i)
    {
        const auto target = scopeData[i];
        const auto coefficient = target > smoothedScope[i] ? displayAttack : displayRelease;
        smoothedScope[i] += (target - smoothedScope[i]) * coefficient;
    }

    repaint();
}

void SpectrumAnalyzerComponent::drawNextFrameOfSpectrum()
{
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    forwardFFT.performFrequencyOnlyForwardTransform (fftData.data());

    constexpr float minDb = -80.0f;
    constexpr float maxDb = 0.0f;
    const auto binHz = (float) (sampleRate / (double) fftSize);

    for (int i = 0; i < scopeSize; ++i)
    {
        const auto proportion = (float) i / (float) (scopeSize - 1);
        const auto freq = proportionToFrequency (proportion);
        const auto bin = juce::jlimit (0, fftSize / 2 - 1, (int) (freq / binHz));

        const auto magnitude = fftData[(size_t) bin] / (float) fftSize;
        const auto db = juce::Decibels::gainToDecibels (magnitude, minDb);
        scopeData[(size_t) i] = juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, 0.0f, 1.0f);
    }
}

juce::Path SpectrumAnalyzerComponent::buildSpectrumPath (juce::Rectangle<float> bounds) const
{
    juce::Path path;

    const auto pointAt = [&] (int index)
    {
        const auto clamped = juce::jlimit (0, scopeSize - 1, index);
        const auto x = bounds.getX() + bounds.getWidth() * (float) clamped / (float) (scopeSize - 1);
        const auto y = bounds.getBottom() - bounds.getHeight() * smoothedScope[(size_t) clamped];
        return juce::Point<float> (x, y);
    };

    // Quadratic segments through the midpoints of consecutive samples: a
    // standard way to get a continuously-curved outline from a dense point
    // series without the overshoot a Catmull-Rom spline would introduce on
    // the near-vertical jumps a spectrum is full of. With 512 points over a
    // few hundred pixels the visible effect is a smooth analogue-looking
    // trace rather than a polyline with visible corners.
    path.startNewSubPath (pointAt (0));
    for (int i = 1; i < scopeSize; ++i)
    {
        const auto previous = pointAt (i - 1);
        const auto point = pointAt (i);
        path.quadraticTo (previous, AbcTrainTheme::midpoint (previous, point));
    }
    path.lineTo (pointAt (scopeSize - 1));

    return path;
}

void SpectrumAnalyzerComponent::paintGrid (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    const auto& theme = AbcTrainTheme::current();

    // Decade-ish frequency markers, the set an engineer actually reads a
    // spectrum against. Deliberately dim: a grid is for orientation when
    // looked for, and should never compete with the curve itself.
    static constexpr float gridFrequencies[] = { 50.0f, 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f };

    g.setFont (AbcTrainLookAndFeel::captionFont());

    for (const auto frequency : gridFrequencies)
    {
        const auto proportion = frequencyToProportion (frequency);
        if (proportion <= 0.0f || proportion >= 1.0f)
            continue;

        const auto x = bounds.getX() + bounds.getWidth() * proportion;

        g.setColour (theme.textDim.withAlpha (0.13f));
        g.drawVerticalLine ((int) x, bounds.getY() + 4.0f, bounds.getBottom() - 4.0f);

        const auto label = frequency >= 1000.0f
                               ? juce::String ((int) (frequency / 1000.0f)) + "k"
                               : juce::String ((int) frequency);

        g.setColour (theme.textDim.withAlpha (0.4f));
        g.drawText (label, juce::Rectangle<float> (x + 3.0f, bounds.getBottom() - 14.0f, 30.0f, 12.0f),
                    juce::Justification::centredLeft, false);
    }

    // Horizontal amplitude guides at quarter steps - unlabelled, purely to
    // give the eye a reference for "how tall is that peak".
    for (int i = 1; i < 4; ++i)
    {
        const auto y = bounds.getY() + bounds.getHeight() * (float) i / 4.0f;
        g.setColour (theme.textDim.withAlpha (0.08f));
        g.drawHorizontalLine ((int) y, bounds.getX(), bounds.getRight());
    }
}

juce::Colour SpectrumAnalyzerComponent::effectiveAccent() const
{
    return accentOverride.isTransparent() ? AbcTrainTheme::current().accent : accentOverride;
}

void SpectrumAnalyzerComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto bounds = getLocalBounds().toFloat();

    // Rounded, and clipped to that rounding - a hard-edged rectangle was
    // the thing that read as raw and unfinished next to everything else.
    g.fillAll (theme.windowBackground);

    juce::Path well;
    well.addRoundedRectangle (bounds, AbcTrainTheme::Radius::well);
    g.setColour (theme.displayBackground);
    g.fillPath (well);

    juce::Graphics::ScopedSaveState clipped (g);
    g.reduceClipRegion (well);

    AbcTrainLookAndFeel::overlayTexture (g, bounds, 0.6f);

    paintGrid (g, bounds);

    const auto curve = buildSpectrumPath (bounds);

    // Close the curve down to the baseline for the fill only - the stroked
    // outline stays open so there's no hard vertical line down each edge.
    auto filled = curve;
    filled.lineTo (bounds.getRight(), bounds.getBottom());
    filled.lineTo (bounds.getX(), bounds.getBottom());
    filled.closeSubPath();

    // Vertical gradient under the curve: strongest where the energy is,
    // fading to nothing at the floor. This is the single biggest visual
    // difference from the old flat 25%-alpha fill - it gives the display
    // depth and makes the loud part of the spectrum read instantly.
    const auto accent = effectiveAccent();

    juce::ColourGradient fillGradient (accent.withAlpha (0.38f), bounds.getCentreX(), bounds.getY(),
                                        accent.withAlpha (0.02f), bounds.getCentreX(), bounds.getBottom(),
                                        false);
    fillGradient.addColour (0.55, accent.withAlpha (0.14f));
    g.setGradientFill (fillGradient);
    g.fillPath (filled);

    // A soft bloom under the outline, then the crisp line on top - the
    // curve reads as lit rather than drawn.
    g.setColour (accent.withAlpha (0.18f));
    g.strokePath (curve, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved));
    g.setColour (accent.brighter (0.15f));
    g.strokePath (curve, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));

    paintOverlay (g, bounds);
}

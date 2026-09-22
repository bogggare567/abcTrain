#include "shared/analysis/SpectrumAnalyzer.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

SpectrumAnalyzerComponent::SpectrumAnalyzerComponent()
    : forwardFFT (fftOrder),
      window (fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    setOpaque (true);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

void SpectrumAnalyzerComponent::pushNextSampleIntoFifo (float sample) noexcept
{
    batch[(size_t) batchCount++] = sample;

    if (batchCount < batchSize)
        return;

    batchCount = 0;

    // Lock-free SPSC write. If the display has fallen behind (the editor is
    // hidden, the machine is busy), the batch is simply dropped: a spectrum
    // is a picture of *now*, and the audio thread must never wait for it.
    if (fifo.getFreeSpace() < batchSize)
        return;

    const auto scope = fifo.write (batchSize);

    if (scope.blockSize1 > 0)
        std::copy (batch.begin(), batch.begin() + scope.blockSize1, fifoStorage.begin() + scope.startIndex1);

    if (scope.blockSize2 > 0)
        std::copy (batch.begin() + scope.blockSize1, batch.begin() + scope.blockSize1 + scope.blockSize2,
                   fifoStorage.begin() + scope.startIndex2);
}

float SpectrumAnalyzerComponent::proportionToFrequency (float proportion) noexcept
{
    return (float) (minFreq * std::pow (maxFreq / minFreq, proportion));
}

float SpectrumAnalyzerComponent::frequencyToProportion (float frequency) noexcept
{
    return (float) (std::log (frequency / minFreq) / std::log (maxFreq / minFreq));
}

float SpectrumAnalyzerComponent::getDisplayedLevelAt (float frequency) const noexcept
{
    const auto index = juce::jlimit (0, scopeSize - 1,
                                     juce::roundToInt (frequencyToProportion (frequency) * (float) (scopeSize - 1)));
    return smoothed[(size_t) index];
}

void SpectrumAnalyzerComponent::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto elapsed = juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001);
    lastTick = now;

    analyse (elapsed);
}

void SpectrumAnalyzerComponent::analyse (double elapsedSeconds)
{
    // 1. Drain the FIFO into the rolling history (message thread only).
    {
        const auto ready = fifo.getNumReady();

        if (ready > 0)
        {
            const auto scope = fifo.read (ready);

            const auto take = [this] (int start, int size)
            {
                for (int i = 0; i < size; ++i)
                {
                    history[(size_t) historyWrite] = fifoStorage[(size_t) (start + i)];
                    historyWrite = (historyWrite + 1) & (fftSize - 1);
                }
            };

            take (scope.startIndex1, scope.blockSize1);
            take (scope.startIndex2, scope.blockSize2);
            pendingNew += ready;
        }
    }

    const auto haveNewAudio = pendingNew > 0;

    // 2. A fresh spectrum from the latest fftSize samples, if anything new
    //    arrived. Nothing new means the source stopped: the target falls to
    //    the floor and the ballistics carry the curve down smoothly.
    if (haveNewAudio)
    {
        pendingNew = 0;

        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = history[(size_t) ((historyWrite + i) & (fftSize - 1))];

        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform (fftData.data());

        const auto binHz = (float) (sampleRate / (double) fftSize);
        const auto numBins = fftSize / 2;
        // Hann window coherent gain is 0.5: a full-scale sine reads 0 dB.
        const auto scale = 2.0f / ((float) fftSize * 0.5f);

        const auto binLevel = [&] (float binPosition)
        {
            const auto b0 = juce::jlimit (0, numBins - 2, (int) binPosition);
            const auto frac = juce::jlimit (0.0f, 1.0f, binPosition - (float) b0);
            return fftData[(size_t) b0] * (1.0f - frac) + fftData[(size_t) b0 + 1] * frac;
        };

        for (int i = 0; i < scopeSize; ++i)
        {
            const auto p = (float) i / (float) (scopeSize - 1);
            const auto half = 0.5f / (float) (scopeSize - 1);
            const auto freq = proportionToFrequency (p);
            const auto lo = proportionToFrequency (juce::jmax (0.0f, p - half)) / binHz;
            const auto hi = proportionToFrequency (juce::jmin (1.0f, p + half)) / binHz;

            float magnitude = 0.0f;

            if (hi - lo < 1.0f)
            {
                magnitude = binLevel (freq / binHz);
            }
            else
            {
                for (int b = (int) lo; b <= juce::jmin (numBins - 1, (int) std::ceil (hi)); ++b)
                    magnitude = juce::jmax (magnitude, fftData[(size_t) b]);
            }

            auto db = juce::Decibels::gainToDecibels (magnitude * scale, floorDb - 30.0f);
            db += tiltDbPerOctave * std::log2 (freq / 1000.0f);
            target[(size_t) i] = juce::jmap (juce::jlimit (floorDb, ceilingDb, db), floorDb, ceilingDb, 0.0f, 1.0f);
        }
    }
    else
    {
        target.fill (0.0f);
    }

    // 3. Ballistics in time, not in frames: 15 ms attack, 300 ms release,
    //    so the curve moves the same at 30 or 60 frames a second. The peak
    //    line holds for a second, then falls at about 20 dB a second.
    const auto dt = (float) elapsedSeconds;
    const auto attack = 1.0f - std::exp (-dt / 0.015f);
    const auto release = 1.0f - std::exp (-dt / 0.30f);
    const auto peakFall = dt * 20.0f / (ceilingDb - floorDb);

    auto moving = haveNewAudio;

    for (size_t i = 0; i < (size_t) scopeSize; ++i)
    {
        const auto before = smoothed[i];
        const auto coefficient = target[i] > smoothed[i] ? attack : release;
        smoothed[i] += (target[i] - smoothed[i]) * coefficient;

        if (smoothed[i] >= peak[i])
        {
            peak[i] = smoothed[i];
            peakHold[i] = 1.0f;
        }
        else if ((peakHold[i] -= dt) <= 0.0f)
        {
            peak[i] = juce::jmax (smoothed[i], peak[i] - peakFall);
        }

        if (std::abs (smoothed[i] - before) > 0.0005f || peak[i] > 0.002f)
            moving = true;
    }

    // Nothing to draw and nothing changing: do not repaint 60 times a
    // second a picture of silence.
    if (moving || ! idle)
        repaint();

    idle = ! moving;
}

juce::Path SpectrumAnalyzerComponent::buildSpectrumPath (const std::array<float, 512>& levels,
                                                         juce::Rectangle<float> bounds) const
{
    juce::Path path;

    const auto pointAt = [&] (int index)
    {
        const auto clamped = juce::jlimit (0, scopeSize - 1, index);
        const auto x = bounds.getX() + bounds.getWidth() * (float) clamped / (float) (scopeSize - 1);
        const auto y = bounds.getBottom() - bounds.getHeight() * 0.94f * levels[(size_t) clamped];
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

    // A well, not a flat rectangle: dark at its top lip, light at its
    // bottom, so the analysis reads as recessed into the panel around it.
    AbcTrainLookAndFeel::paintRecessedWell (g, bounds, AbcTrainTheme::Radius::well);

    juce::Path well;
    well.addRoundedRectangle (bounds, AbcTrainTheme::Radius::well);

    juce::Graphics::ScopedSaveState clipped (g);
    g.reduceClipRegion (well);

    AbcTrainLookAndFeel::overlayTexture (g, bounds, 0.6f);

    paintGrid (g, bounds);

    const auto curve = buildSpectrumPath (smoothed, bounds);

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

    // The peak line: where it was loudest a moment ago, faint and thin, so
    // a transient that has already gone is still there to read.
    g.setColour (theme.textBright.withAlpha (0.28f));
    g.strokePath (buildSpectrumPath (peak, bounds), juce::PathStrokeType (1.0f, juce::PathStrokeType::curved));

    paintOverlay (g, bounds);
}

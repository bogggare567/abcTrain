#include "WaveformDisplay.h"
#include "AbcTrainLookAndFeel.h"
#include "AbcTrainTheme.h"
#include <algorithm>

int WaveformDisplay::drain()
{
    const auto ready = fifo.getNumReady();

    if (ready <= 0)
        return 0;

    const auto n = juce::jmin (ready, numColumns);
    const auto scope = fifo.read (ready);

    // Scroll left by as many columns as arrived.
    const auto shift = [n] (std::array<float, numColumns>& a)
    {
        std::rotate (a.begin(), a.begin() + n, a.end());
    };

    shift (inputPeaks); shift (outputPeaks); shift (inputRms); shift (outputRms); shift (highlights);

    // Only the newest `n` of what arrived fit on screen.
    auto writeAt = numColumns - n;
    auto skip = ready - n;
    float newInput = 0.0f, newOutput = 0.0f, newHighlight = 0.0f;

    const auto take = [&] (int start, int size)
    {
        for (int i = 0; i < size; ++i)
        {
            const auto& c = columns[(size_t) (start + i)];
            newInput = juce::jmax (newInput, c.inputPeak);
            newOutput = juce::jmax (newOutput, c.outputPeak);
            newHighlight = juce::jmax (newHighlight, c.highlight);

            if (skip > 0) { --skip; continue; }

            inputPeaks[(size_t) writeAt] = c.inputPeak;
            outputPeaks[(size_t) writeAt] = c.outputPeak;
            inputRms[(size_t) writeAt] = c.inputRms;
            outputRms[(size_t) writeAt] = c.outputRms;
            highlights[(size_t) writeAt] = c.highlight;
            ++writeAt;
        }
    };

    take (scope.startIndex1, scope.blockSize1);
    take (scope.startIndex2, scope.blockSize2);

    inputPeakReadout = juce::jmax (inputPeakReadout, newInput);
    outputPeakReadout = juce::jmax (outputPeakReadout, newOutput);
    highlightReadout = juce::jmax (highlightReadout * 0.6f, newHighlight);
    return ready;
}

void WaveformDisplay::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto dt = (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001);
    lastTick = now;

    // Peak readouts fall at about 20 dB a second once the signal drops, the
    // way a meter does - numbers you can read, not a flicker.
    const auto fall = std::pow (10.0f, -20.0f * dt / 20.0f);
    inputPeakReadout *= fall;
    outputPeakReadout *= fall;

    const auto arrived = drain();

    if (arrived == 0)
    {
        highlightReadout *= fall;

        // Nothing new: the picture is frozen, so only repaint while
        // something on screen is still moving.
        if (quiet)
            return;

        quiet = inputPeakReadout < 1.0e-4f && outputPeakReadout < 1.0e-4f;

        // With no audio the scroll stops rather than inventing silence -
        // a paused transport should leave the last picture readable.
        repaint();
        return;
    }

    quiet = false;
    repaint();
}

juce::Path WaveformDisplay::buildEnvelope (const std::array<float, numColumns>& values,
                                           juce::Rectangle<float> bounds) const
{
    // One closed shape, symmetric about the zero line: along the top edge
    // left to right, back along the mirrored bottom edge right to left.
    // Midpoint-quadratic segments, so 400 columns read as one waveform
    // body rather than a bar chart.
    juce::Path path;
    const auto midY = bounds.getCentreY();
    const auto half = bounds.getHeight() * 0.5f * 0.92f;
    const auto step = bounds.getWidth() / (float) (numColumns - 1);

    const auto point = [&] (int i, bool mirrored)
    {
        const auto a = juce::jlimit (0.0f, 1.0f, values[(size_t) i]) * half;
        return juce::Point<float> (bounds.getX() + step * (float) i, mirrored ? midY + a : midY - a);
    };

    path.startNewSubPath (point (0, false));

    for (int i = 1; i < numColumns; ++i)
        path.quadraticTo (point (i - 1, false), AbcTrainTheme::midpoint (point (i - 1, false), point (i, false)));

    path.lineTo (point (numColumns - 1, false));
    path.lineTo (point (numColumns - 1, true));

    for (int i = numColumns - 2; i >= 0; --i)
        path.quadraticTo (point (i + 1, true), AbcTrainTheme::midpoint (point (i + 1, true), point (i, true)));

    path.lineTo (point (0, true));
    path.closeSubPath();
    return path;
}

juce::Colour WaveformDisplay::effectiveAccent() const
{
    return accentOverride.isTransparent() ? AbcTrainTheme::current().accent : accentOverride;
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto bounds = getLocalBounds().toFloat();

    g.fillAll (theme.windowBackground);
    AbcTrainLookAndFeel::paintRecessedWell (g, bounds.reduced (0.5f), AbcTrainTheme::Radius::well);
    AbcTrainLookAndFeel::overlayTexture (g, bounds, 0.6f);

    const auto plot = bounds.reduced (1.0f, 4.0f);
    const auto midY = plot.getCentreY();

    // Faint level guides at -6 and -18 dBFS, and the zero line.
    g.setColour (theme.textDim.withAlpha (0.07f));

    for (const auto db : { -6.0f, -18.0f })
    {
        const auto a = juce::Decibels::decibelsToGain (db) * plot.getHeight() * 0.5f * 0.92f;
        g.fillRect (plot.getX(), midY - a, plot.getWidth(), 1.0f);
        g.fillRect (plot.getX(), midY + a, plot.getWidth(), 1.0f);
    }

    g.setColour (theme.textDim.withAlpha (0.2f));
    g.fillRect (plot.getX(), midY, plot.getWidth(), 1.0f);

    // The input: a dim silhouette behind the output, so what the plugin
    // took away is the grey that shows around the colour.
    g.setColour (theme.text.withAlpha (0.10f));
    g.fillPath (buildEnvelope (inputPeaks, plot));

    // The output, tinted by how hard the plugin is working right now.
    const auto highlightProportion = juce::jlimit (0.0f, highlightRangeDb, highlightReadout) / highlightRangeDb;
    const auto colour = effectiveAccent().interpolatedWith (theme.negative, highlightProportion);

    const auto peakShape = buildEnvelope (outputPeaks, plot);
    const auto rmsShape = buildEnvelope (outputRms, plot);

    juce::ColourGradient peakFill (colour.withAlpha (0.30f), plot.getCentreX(), plot.getY(),
                                   colour.withAlpha (0.30f), plot.getCentreX(), plot.getBottom(), false);
    peakFill.addColour (0.5, colour.withAlpha (0.08f));
    g.setGradientFill (peakFill);
    g.fillPath (peakShape);

    // The RMS body: brighter, and it is the part you hear as loudness.
    juce::ColourGradient rmsFill (colour.withAlpha (0.75f), plot.getCentreX(), plot.getY(),
                                  colour.withAlpha (0.75f), plot.getCentreX(), plot.getBottom(), false);
    rmsFill.addColour (0.5, colour.withAlpha (0.35f));
    g.setGradientFill (rmsFill);
    g.fillPath (rmsShape);

    if (highlightProportion > 0.01f)
    {
        g.setColour (colour.withAlpha (0.25f * highlightProportion));
        g.strokePath (peakShape, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved));
    }

    g.setColour (colour.brighter (0.15f).withAlpha (0.9f));
    g.strokePath (peakShape, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved));

    // The newest edge fades in rather than ending on a hard line.
    const auto edge = plot.withLeft (plot.getRight() - 18.0f);
    g.setGradientFill (juce::ColourGradient (theme.windowBackground.withAlpha (0.0f), edge.getX(), 0.0f,
                                             theme.windowBackground.withAlpha (0.35f), edge.getRight(), 0.0f, false));
    g.fillRect (edge);
}

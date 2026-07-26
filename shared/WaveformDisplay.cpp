#include "WaveformDisplay.h"
#include "AbcTrainLookAndFeel.h"
#include "AbcTrainTheme.h"
#include <algorithm>

void WaveformDisplay::timerCallback()
{
    std::rotate (inputHistory.begin(), inputHistory.begin() + 1, inputHistory.end());
    std::rotate (outputHistory.begin(), outputHistory.begin() + 1, outputHistory.end());
    std::rotate (highlightHistory.begin(), highlightHistory.begin() + 1, highlightHistory.end());

    inputHistory.back() = columnInputPeak;
    outputHistory.back() = columnOutputPeak;
    highlightHistory.back() = columnMaxHighlight;

    lastInputPeak = columnInputPeak;
    lastOutputPeak = columnOutputPeak;
    lastHighlight = columnMaxHighlight;

    columnInputPeak = 0.0f;
    columnOutputPeak = 0.0f;
    columnMaxHighlight = 0.0f;

    repaint();
}

juce::Point<float> WaveformDisplay::envelopePoint (const std::array<float, numColumns>& history,
                                                   juce::Rectangle<float> bounds, int index, bool mirrored)
{
    const auto clamped = juce::jlimit (0, numColumns - 1, index);
    const auto columnWidth = bounds.getWidth() / (float) (numColumns - 1);
    const auto amplitude = juce::jlimit (0.0f, 1.5f, history[(size_t) clamped]) * bounds.getHeight() * 0.5f;
    const auto midY = bounds.getCentreY();

    return { bounds.getX() + columnWidth * (float) clamped,
             mirrored ? midY + amplitude : midY - amplitude };
}

juce::Path WaveformDisplay::buildEnvelopePath (const std::array<float, numColumns>& history,
                                                juce::Rectangle<float> bounds, bool mirrored) const
{
    juce::Path path;

    // Same midpoint-quadratic smoothing as the spectrum curve, for the same
    // reason: 100 peak columns drawn as a polyline reads as a bar chart,
    // and drawn as a smooth envelope reads as a waveform.
    path.startNewSubPath (envelopePoint (history, bounds, 0, mirrored));
    for (int i = 1; i < numColumns; ++i)
    {
        const auto previous = envelopePoint (history, bounds, i - 1, mirrored);
        const auto point = envelopePoint (history, bounds, i, mirrored);
        path.quadraticTo (previous, AbcTrainTheme::midpoint (previous, point));
    }
    path.lineTo (envelopePoint (history, bounds, numColumns - 1, mirrored));

    return path;
}

juce::Path WaveformDisplay::buildEnvelopeShape (const std::array<float, numColumns>& history,
                                                 juce::Rectangle<float> bounds) const
{
    // One closed path: out along the top edge left-to-right, back along the
    // mirrored bottom edge right-to-left. Appending two separate
    // left-to-right subpaths instead would leave the shape unclosed and
    // fill as two overlapping wedges rather than one waveform body.
    juce::Path path;

    path.startNewSubPath (envelopePoint (history, bounds, 0, false));
    for (int i = 1; i < numColumns; ++i)
    {
        const auto previous = envelopePoint (history, bounds, i - 1, false);
        const auto point = envelopePoint (history, bounds, i, false);
        path.quadraticTo (previous, AbcTrainTheme::midpoint (previous, point));
    }
    path.lineTo (envelopePoint (history, bounds, numColumns - 1, false));

    path.lineTo (envelopePoint (history, bounds, numColumns - 1, true));
    for (int i = numColumns - 2; i >= 0; --i)
    {
        const auto previous = envelopePoint (history, bounds, i + 1, true);
        const auto point = envelopePoint (history, bounds, i, true);
        path.quadraticTo (previous, AbcTrainTheme::midpoint (previous, point));
    }
    path.lineTo (envelopePoint (history, bounds, 0, true));

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

    g.fillAll (theme.displayBackground);
    AbcTrainLookAndFeel::overlayTexture (g, bounds, 0.6f);

    const auto midY = bounds.getCentreY();

    // Zero line first, so the traces sit on top of it.
    g.setColour (theme.textDim.withAlpha (0.18f));
    g.drawHorizontalLine ((int) midY, bounds.getX(), bounds.getRight());

    // --- input trace: a dim silhouette behind the output ---
    const auto inputTop = buildEnvelopePath (inputHistory, bounds, false);
    const auto inputBottom = buildEnvelopePath (inputHistory, bounds, true);

    g.setColour (theme.text.withAlpha (0.10f));
    g.fillPath (buildEnvelopeShape (inputHistory, bounds));
    g.setColour (theme.text.withAlpha (0.28f));
    g.strokePath (inputTop, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved));
    g.strokePath (inputBottom, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved));

    // --- output trace: filled, tinted by how hard the plugin is working ---
    // The tint is sampled from the most recent column rather than per
    // column, since a gradient-per-column fill would cost a separate path
    // per column and defeat the point of drawing one smooth envelope.
    const auto highlightProportion = juce::jlimit (0.0f, highlightRangeDb, lastHighlight) / highlightRangeDb;
    const auto traceColour = effectiveAccent().interpolatedWith (theme.negative, highlightProportion);

    const auto outputTop = buildEnvelopePath (outputHistory, bounds, false);
    const auto outputBottom = buildEnvelopePath (outputHistory, bounds, true);
    const auto outputShape = buildEnvelopeShape (outputHistory, bounds);

    juce::ColourGradient fillGradient (traceColour.withAlpha (0.42f), bounds.getCentreX(), bounds.getY(),
                                        traceColour.withAlpha (0.06f), bounds.getCentreX(), midY, false);
    g.setGradientFill (fillGradient);
    g.fillPath (outputShape);

    // Glow under the outline that intensifies with the highlight amount -
    // when a compressor is really working, its waveform visibly heats up.
    if (highlightProportion > 0.01f)
    {
        g.setColour (traceColour.withAlpha (0.22f * highlightProportion));
        g.strokePath (outputTop, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved));
        g.strokePath (outputBottom, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved));
    }

    g.setColour (traceColour.brighter (0.1f));
    g.strokePath (outputTop, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved));
    g.strokePath (outputBottom, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved));
}

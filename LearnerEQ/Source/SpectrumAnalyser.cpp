#include "SpectrumAnalyser.h"
#include "EQCoefficients.h"
#include "FrequencyGuide.h"
#include "../../shared/AbcTrainTheme.h"

void SpectrumAnalyserComponent::setEQState (double sampleRate, const std::array<float, 4>& freqs,
                                             const std::array<float, 4>& gains, const std::array<float, 4>& qs)
{
    eqSampleRate = sampleRate;
    eqFreqs = freqs;
    eqGains = gains;
    eqQs = qs;
    setSampleRate (sampleRate);
    repaint();
}

void SpectrumAnalyserComponent::setHighlightedBand (int bandIndex) noexcept
{
    highlightedBandIndex = bandIndex;
    repaint();
}

juce::Path SpectrumAnalyserComponent::buildResponseCurvePath (juce::Rectangle<float> bounds) const
{
    constexpr float maxDb = 18.0f;
    constexpr int numPoints = 512;
    juce::Path path;

    for (int i = 0; i < numPoints; ++i)
    {
        const auto proportion = (float) i / (float) (numPoints - 1);
        const auto freq = FrequencyGuide::proportionToFrequency (proportion);

        float totalDb = 0.0f;
        for (int band = 0; band < 4; ++band)
        {
            const auto coeffs = EQCoefficients::make (band, eqSampleRate, eqFreqs[(size_t) band],
                                                       eqGains[(size_t) band], eqQs[(size_t) band]);
            totalDb += juce::Decibels::gainToDecibels (coeffs->getMagnitudeForFrequency ((double) freq, eqSampleRate));
        }

        const auto x = bounds.getX() + bounds.getWidth() * proportion;
        const auto y = bounds.getY() + bounds.getHeight() * (0.5f - juce::jlimit (-maxDb, maxDb, totalDb) / (2.0f * maxDb));

        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    return path;
}

void SpectrumAnalyserComponent::paintOverlay (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto& theme = AbcTrainTheme::current();

    if (highlightedBandIndex >= 0)
    {
        const auto freq = eqFreqs[(size_t) highlightedBandIndex];
        const auto q = juce::jmax (0.1f, eqQs[(size_t) highlightedBandIndex]);
        const auto bandwidth = freq / q;
        const auto loProportion = FrequencyGuide::frequencyToProportion (juce::jmax (FrequencyGuide::minFreq, freq - bandwidth * 0.5f));
        const auto hiProportion = FrequencyGuide::frequencyToProportion (juce::jmin (FrequencyGuide::maxFreq, freq + bandwidth * 0.5f));

        const auto x0 = bounds.getX() + bounds.getWidth() * loProportion;
        const auto x1 = bounds.getX() + bounds.getWidth() * hiProportion;
        const auto region = juce::Rectangle<float> (x0, bounds.getY(), x1 - x0, bounds.getHeight());

        // The highlighted band fades out toward its edges rather than
        // being a hard-edged block - a bell's influence doesn't stop
        // abruptly at its bandwidth, and neither should the way it's drawn.
        juce::ColourGradient bandGradient (theme.accentWarm.withAlpha (0.03f), region.getX(), region.getY(),
                                            theme.accentWarm.withAlpha (0.03f), region.getRight(), region.getY(),
                                            false);
        bandGradient.addColour (0.5, theme.accentWarm.withAlpha (0.22f));
        g.setGradientFill (bandGradient);
        g.fillRect (region);
    }

    const auto curve = buildResponseCurvePath (bounds);

    // Bloom beneath, crisp line on top - same treatment as the spectrum
    // curve itself, so the two read as one family of graphics.
    g.setColour (theme.textBright.withAlpha (0.16f));
    g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
    g.setColour (theme.textBright);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
}

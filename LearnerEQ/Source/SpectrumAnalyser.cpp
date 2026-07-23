#include "SpectrumAnalyser.h"
#include "EQCoefficients.h"
#include "FrequencyGuide.h"

SpectrumAnalyserComponent::SpectrumAnalyserComponent()
    : forwardFFT (fftOrder),
      window (fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    setOpaque (true);
    startTimerHz (30);
}

void SpectrumAnalyserComponent::pushNextSampleIntoFifo (float sample) noexcept
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

void SpectrumAnalyserComponent::setEQState (double sampleRate, const std::array<float, 4>& freqs,
                                             const std::array<float, 4>& gains, const std::array<float, 4>& qs)
{
    eqSampleRate = sampleRate;
    eqFreqs = freqs;
    eqGains = gains;
    eqQs = qs;
    repaint();
}

void SpectrumAnalyserComponent::setHighlightedBand (int bandIndex) noexcept
{
    highlightedBandIndex = bandIndex;
    repaint();
}

void SpectrumAnalyserComponent::timerCallback()
{
    if (nextFFTBlockReady)
    {
        drawNextFrameOfSpectrum();
        nextFFTBlockReady = false;
    }

    repaint();
}

void SpectrumAnalyserComponent::drawNextFrameOfSpectrum()
{
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    forwardFFT.performFrequencyOnlyForwardTransform (fftData.data());

    constexpr float minDb = -80.0f;
    constexpr float maxDb = 0.0f;
    const auto binHz = (float) (eqSampleRate / (double) fftSize);

    for (int i = 0; i < scopeSize; ++i)
    {
        const auto proportion = (float) i / (float) (scopeSize - 1);
        const auto freq = FrequencyGuide::proportionToFrequency (proportion);
        const auto bin = juce::jlimit (0, fftSize / 2 - 1, (int) (freq / binHz));

        const auto magnitude = fftData[(size_t) bin] / (float) fftSize;
        const auto db = juce::Decibels::gainToDecibels (magnitude, minDb);
        scopeData[(size_t) i] = juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, 0.0f, 1.0f);
    }
}

juce::Path SpectrumAnalyserComponent::buildResponseCurvePath (juce::Rectangle<float> bounds) const
{
    constexpr float maxDb = 18.0f;
    juce::Path path;

    for (int i = 0; i < scopeSize; ++i)
    {
        const auto proportion = (float) i / (float) (scopeSize - 1);
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

void SpectrumAnalyserComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff14141a));

    juce::Path spectrumPath;
    spectrumPath.startNewSubPath (bounds.getX(), bounds.getBottom());
    for (int i = 0; i < scopeSize; ++i)
    {
        const auto x = bounds.getX() + bounds.getWidth() * (float) i / (float) (scopeSize - 1);
        const auto y = bounds.getY() + bounds.getHeight() * (1.0f - scopeData[(size_t) i]);
        spectrumPath.lineTo (x, y);
    }
    spectrumPath.lineTo (bounds.getRight(), bounds.getBottom());
    spectrumPath.closeSubPath();

    g.setColour (juce::Colours::deepskyblue.withAlpha (0.25f));
    g.fillPath (spectrumPath);
    g.setColour (juce::Colours::deepskyblue);
    g.strokePath (spectrumPath, juce::PathStrokeType (1.0f));

    if (highlightedBandIndex >= 0)
    {
        const auto freq = eqFreqs[(size_t) highlightedBandIndex];
        const auto q = juce::jmax (0.1f, eqQs[(size_t) highlightedBandIndex]);
        const auto bandwidth = freq / q;
        const auto loProportion = FrequencyGuide::frequencyToProportion (juce::jmax (FrequencyGuide::minFreq, freq - bandwidth * 0.5f));
        const auto hiProportion = FrequencyGuide::frequencyToProportion (juce::jmin (FrequencyGuide::maxFreq, freq + bandwidth * 0.5f));

        const auto x0 = bounds.getX() + bounds.getWidth() * loProportion;
        const auto x1 = bounds.getX() + bounds.getWidth() * hiProportion;
        g.setColour (juce::Colours::orange.withAlpha (0.18f));
        g.fillRect (juce::Rectangle<float> (x0, bounds.getY(), x1 - x0, bounds.getHeight()));
    }

    g.setColour (juce::Colours::white);
    g.strokePath (buildResponseCurvePath (bounds), juce::PathStrokeType (2.0f));
}

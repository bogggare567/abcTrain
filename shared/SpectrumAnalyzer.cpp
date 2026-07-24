#include "SpectrumAnalyzer.h"

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

void SpectrumAnalyzerComponent::timerCallback()
{
    if (nextFFTBlockReady)
    {
        drawNextFrameOfSpectrum();
        nextFFTBlockReady = false;
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

void SpectrumAnalyzerComponent::paint (juce::Graphics& g)
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

    paintOverlay (g, bounds);
}

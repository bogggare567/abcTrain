#include "WaveformDisplay.h"
#include <algorithm>

void WaveformDisplay::timerCallback()
{
    std::rotate (inputHistory.begin(), inputHistory.begin() + 1, inputHistory.end());
    std::rotate (outputHistory.begin(), outputHistory.begin() + 1, outputHistory.end());
    std::rotate (reductionHistory.begin(), reductionHistory.begin() + 1, reductionHistory.end());

    inputHistory.back() = columnInputPeak;
    outputHistory.back() = columnOutputPeak;
    reductionHistory.back() = columnMaxReductionDb;

    lastInputPeak = columnInputPeak;
    lastOutputPeak = columnOutputPeak;
    lastReductionDb = columnMaxReductionDb;

    columnInputPeak = 0.0f;
    columnOutputPeak = 0.0f;
    columnMaxReductionDb = 0.0f;

    repaint();
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff14141a));

    const auto midY = bounds.getCentreY();
    const auto columnWidth = bounds.getWidth() / (float) numColumns;

    for (int i = 0; i < numColumns; ++i)
    {
        const auto x = bounds.getX() + columnWidth * (float) i;

        const auto inH = juce::jlimit (0.0f, 1.5f, inputHistory[(size_t) i]) * bounds.getHeight() * 0.5f;
        g.setColour (juce::Colours::grey.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<float> (x, midY - inH, columnWidth * 0.5f, inH * 2.0f));

        const auto outH = juce::jlimit (0.0f, 1.5f, outputHistory[(size_t) i]) * bounds.getHeight() * 0.5f;
        const auto reductionProportion = juce::jlimit (0.0f, 24.0f, reductionHistory[(size_t) i]) / 24.0f;
        const auto colour = juce::Colours::deepskyblue.interpolatedWith (juce::Colours::red, reductionProportion);
        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (x + columnWidth * 0.5f, midY - outH, columnWidth * 0.5f, outH * 2.0f));
    }

    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawHorizontalLine ((int) midY, bounds.getX(), bounds.getRight());
}

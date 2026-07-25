#include "WaveformDisplay.h"
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

void WaveformDisplay::paint (juce::Graphics& g)
{
    // Theme-consistent tones (see AbcTrainLookAndFeel's scheme) instead
    // of JUCE's stock Colours::grey/deepskyblue/red/white, which read as
    // flatter and more generic against this project's otherwise
    // deliberate dark palette.
    const juce::Colour accentBlue { 0xff5b9bd5 };
    const juce::Colour highlightRed { 0xffd9615f };
    const juce::Colour bodyText { 0xffe0e0e0 };

    const auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff14141a));

    const auto midY = bounds.getCentreY();
    const auto columnWidth = bounds.getWidth() / (float) numColumns;

    for (int i = 0; i < numColumns; ++i)
    {
        const auto x = bounds.getX() + columnWidth * (float) i;

        const auto inH = juce::jlimit (0.0f, 1.5f, inputHistory[(size_t) i]) * bounds.getHeight() * 0.5f;
        g.setColour (bodyText.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (x, midY - inH, columnWidth * 0.5f, inH * 2.0f));

        const auto outH = juce::jlimit (0.0f, 1.5f, outputHistory[(size_t) i]) * bounds.getHeight() * 0.5f;
        const auto highlightProportion = juce::jlimit (0.0f, highlightRangeDb, highlightHistory[(size_t) i]) / highlightRangeDb;
        const auto colour = accentBlue.interpolatedWith (highlightRed, highlightProportion);
        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (x + columnWidth * 0.5f, midY - outH, columnWidth * 0.5f, outH * 2.0f));
    }

    g.setColour (bodyText.withAlpha (0.2f));
    g.drawHorizontalLine ((int) midY, bounds.getX(), bounds.getRight());
}

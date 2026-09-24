#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

// Scrolling dual waveform: the input as a dim silhouette, the output
// filled in the plugin's colour and tinted toward red by `highlightAmount`
// (LearnerComp passes its gain reduction in dB; 0 means no tint).
//
// Threading (ADR 038). The audio thread accumulates one *column* - the
// peak and the RMS of `samplesPerColumn` samples - in members only it
// touches, and hands each finished column to the message thread through a
// lock-free SPSC FIFO. The first version accumulated into floats the
// timer also read and zeroed, unsynchronised: a data race, "visually
// harmless" only until a compiler decided otherwise. The readouts
// (getInputPeak and friends) are message-thread values derived from the
// drained columns.
//
// Why it looks smooth now: 400 columns of 256 samples (about 2.3 s at
// 44.1 kHz) arrive about 170 times a second and are drained 60 times a
// second, so the picture scrolls a few pixels at a time instead of
// jumping a 33 ms column at once; and each column carries its RMS as well
// as its peak, drawn as a brighter body inside the peak outline - the
// body is what you hear as loudness, the outline is what the meter sees.
class WaveformDisplay : public juce::Component,
                         private juce::Timer
{
public:
    WaveformDisplay() { startTimerHz (60); }

    static constexpr int numColumns = 400;
    static constexpr int samplesPerColumn = 256;
    static constexpr float highlightRangeDb = 24.0f;

    // Audio thread. Real-time safe: arithmetic on audio-thread-only
    // members, and one lock-free FIFO write per 256 samples.
    void pushSample (float inputSample, float outputSample, float highlightAmount = 0.0f) noexcept
    {
        if (resetRequested.exchange (false))
            accumulator = {};

        const auto in = std::abs (inputSample);
        const auto out = std::abs (outputSample);
        accumulator.inputPeak = juce::jmax (accumulator.inputPeak, in);
        accumulator.outputPeak = juce::jmax (accumulator.outputPeak, out);
        accumulator.inputSquares += in * in;
        accumulator.outputSquares += out * out;
        accumulator.highlight = juce::jmax (accumulator.highlight, highlightAmount);

        if (++accumulator.count < samplesPerColumn)
            return;

        Column column;
        column.inputPeak = accumulator.inputPeak;
        column.outputPeak = accumulator.outputPeak;
        column.inputRms = std::sqrt (accumulator.inputSquares / (float) accumulator.count);
        column.outputRms = std::sqrt (accumulator.outputSquares / (float) accumulator.count);
        column.highlight = accumulator.highlight;
        accumulator = {};

        if (fifo.getFreeSpace() > 0)
        {
            const auto scope = fifo.write (1);
            columns[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)] = column;
        }
    }

    // Message thread: the readouts, with a meter's release so the numbers
    // can be read rather than flicker.
    float getInputPeak() const noexcept { return inputPeakReadout; }
    float getOutputPeak() const noexcept { return outputPeakReadout; }
    float getCurrentHighlightAmount() const noexcept { return highlightReadout; }

    void setAccentColour (juce::Colour newAccent) { accentOverride = newAccent; repaint(); }

    // Wipe the scroll history. The trainer buys its hint per round, and a
    // display still holding the previous round's shape would be showing
    // the answer to a question already scored. Message thread; the audio
    // thread drops its half-built column when it next runs.
    void reset() noexcept
    {
        resetRequested = true;
        fifo.reset();
        inputPeaks.fill (0.0f);
        outputPeaks.fill (0.0f);
        inputRms.fill (0.0f);
        outputRms.fill (0.0f);
        highlights.fill (0.0f);
        inputPeakReadout = outputPeakReadout = highlightReadout = 0.0f;
        repaint();
    }

    // For tests: drain whatever the audio side has produced.
    int drainForTest() { return drain(); }


    // Drawn straight onto the page, without its own well: for the trainer's
    // hint, which sits on the page background (possibly the player's own
    // picture) instead of in a box of its own.
    void setBackdropVisible (bool shouldShow) { backdrop = shouldShow; setOpaque (shouldShow && opaqueWithBackdrop); repaint(); }
    void paint (juce::Graphics&) override;

    // Draw the per-column highlight as a gain-reduction line hanging from
    // the top, with a caption, and a legend along the bottom - the Comp's
    // display. Empty strings turn it off.
    void setGainReductionTrace (juce::String captionText, juce::String legendText)
    {
        grCaption = std::move (captionText);
        legend = std::move (legendText);
        repaint();
    }

private:
    struct Column
    {
        float inputPeak = 0.0f, outputPeak = 0.0f, inputRms = 0.0f, outputRms = 0.0f, highlight = 0.0f;
    };

    struct Accumulator
    {
        float inputPeak = 0.0f, outputPeak = 0.0f, inputSquares = 0.0f, outputSquares = 0.0f, highlight = 0.0f;
        int count = 0;
    };

    juce::Colour effectiveAccent() const;
    void timerCallback() override;
    int drain();

    juce::Path buildEnvelope (const std::array<float, numColumns>& values, juce::Rectangle<float> bounds) const;

    juce::Colour accentOverride { juce::Colours::transparentBlack };

    // Audio thread only.
    Accumulator accumulator;
    std::atomic<bool> resetRequested { false };

    // Audio -> message thread.
    static constexpr int fifoCapacity = 1024;
    juce::AbstractFifo fifo { fifoCapacity };
    std::array<Column, (size_t) fifoCapacity> columns {};

    // Message thread only.
    std::array<float, numColumns> inputPeaks {}, outputPeaks {}, inputRms {}, outputRms {}, highlights {};
    float inputPeakReadout = 0.0f, outputPeakReadout = 0.0f, highlightReadout = 0.0f;
    juce::String grCaption, legend;
    double lastTick = 0.0;
    bool quiet = true;

    bool backdrop = true;
    bool opaqueWithBackdrop = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};

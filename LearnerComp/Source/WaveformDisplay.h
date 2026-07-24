#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// Scrolling peak-based dual waveform: input trace (gray) and output trace
// (blue, tinted red where the compressor is actively reducing gain),
// updated via a 30 Hz UI timer - the same FIFO-accumulate/timer-flush
// pattern LearnerEQ's SpectrumAnalyserComponent uses for its FFT, just
// without the FFT. Each column shows the peak absolute value seen during
// that ~33 ms window, not a true sample-accurate waveform - enough to see
// level and compression activity over the last few seconds without
// storing/downsampling every raw sample.
//
// pushSample() is called from the audio thread; the column accumulators
// and the last-peak values it produces are read from the message thread
// with no lock. The only race is "a repaint uses a still-accumulating
// column," which is visually harmless for a meter/waveform display - same
// reasoning as SpectrumAnalyserComponent's FIFO.
class WaveformDisplay : public juce::Component,
                         private juce::Timer
{
public:
    WaveformDisplay() { startTimerHz (30); }

    static constexpr int numColumns = 100;

    void pushSample (float inputSample, float outputSample, float gainReductionDb) noexcept
    {
        columnInputPeak = juce::jmax (columnInputPeak, std::abs (inputSample));
        columnOutputPeak = juce::jmax (columnOutputPeak, std::abs (outputSample));
        columnMaxReductionDb = juce::jmax (columnMaxReductionDb, gainReductionDb);
    }

    float getInputPeak() const noexcept { return lastInputPeak; }
    float getOutputPeak() const noexcept { return lastOutputPeak; }
    float getCurrentGainReductionDb() const noexcept { return lastReductionDb; }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::array<float, numColumns> inputHistory {};
    std::array<float, numColumns> outputHistory {};
    std::array<float, numColumns> reductionHistory {};

    float columnInputPeak = 0.0f;
    float columnOutputPeak = 0.0f;
    float columnMaxReductionDb = 0.0f;

    float lastInputPeak = 0.0f;
    float lastOutputPeak = 0.0f;
    float lastReductionDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// Scrolling peak-based dual waveform: input trace (gray) and output trace
// (blue, tinted toward red proportional to `highlightAmount`), updated via
// a 30 Hz UI timer - the same FIFO-accumulate/timer-flush pattern
// LearnerEQ's SpectrumAnalyserComponent uses for its FFT, just without the
// FFT. Each column shows the peak absolute value seen during that ~33 ms
// window, not a true sample-accurate waveform - enough to see level and
// processing activity over the last few seconds without storing/
// downsampling every raw sample.
//
// `highlightAmount` means whatever the caller wants tinted red: LearnerComp
// passes the current gain-reduction in dB; a plugin with no such concept
// can just pass 0 (or omit it) to disable tinting. Shared between
// LearnerComp and LearnerVerb - extracted here once a second consumer
// needed the same shape, rather than guessing the abstraction from one.
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
    static constexpr float highlightRangeDb = 24.0f;

    void pushSample (float inputSample, float outputSample, float highlightAmount = 0.0f) noexcept
    {
        columnInputPeak = juce::jmax (columnInputPeak, std::abs (inputSample));
        columnOutputPeak = juce::jmax (columnOutputPeak, std::abs (outputSample));
        columnMaxHighlight = juce::jmax (columnMaxHighlight, highlightAmount);
    }

    float getInputPeak() const noexcept { return lastInputPeak; }
    float getOutputPeak() const noexcept { return lastOutputPeak; }
    float getCurrentHighlightAmount() const noexcept { return lastHighlight; }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::array<float, numColumns> inputHistory {};
    std::array<float, numColumns> outputHistory {};
    std::array<float, numColumns> highlightHistory {};

    float columnInputPeak = 0.0f;
    float columnOutputPeak = 0.0f;
    float columnMaxHighlight = 0.0f;

    float lastInputPeak = 0.0f;
    float lastOutputPeak = 0.0f;
    float lastHighlight = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};

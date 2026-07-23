#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// Live spectrum display + EQ response curve overlay + highlighted-band
// overlay while the user is dragging a band's frequency control.
//
// Follows JUCE's standard FFT-analyser pattern: processBlock (audio
// thread) pushes samples into a fixed-size FIFO via
// pushNextSampleIntoFifo(); a UI timer runs the FFT and repaints once a
// full block has accumulated. The FIFO write/read indices are plain ints
// with no lock - the only race is "a repaint uses a half-overwritten
// buffer," which is visually harmless for a meter display.
class SpectrumAnalyserComponent : public juce::Component,
                                   private juce::Timer
{
public:
    SpectrumAnalyserComponent();

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    void pushNextSampleIntoFifo (float sample) noexcept;

    // Called from the message thread once per frame with the current
    // parameter values, so the response curve tracks knob movement even
    // when no audio is playing.
    void setEQState (double sampleRate, const std::array<float, 4>& freqs,
                      const std::array<float, 4>& gains, const std::array<float, 4>& qs);

    void setHighlightedBand (int bandIndex) noexcept;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void drawNextFrameOfSpectrum();
    juce::Path buildResponseCurvePath (juce::Rectangle<float> bounds) const;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, (size_t) fftSize> fifo {};
    std::array<float, (size_t) fftSize * 2> fftData {};
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;

    static constexpr int scopeSize = 512;
    std::array<float, (size_t) scopeSize> scopeData {};

    double eqSampleRate = 44100.0;
    std::array<float, 4> eqFreqs { 100.0f, 800.0f, 3000.0f, 8000.0f };
    std::array<float, 4> eqGains {};
    std::array<float, 4> eqQs { 0.7f, 0.7f, 0.7f, 0.7f };
    int highlightedBandIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyserComponent)
};

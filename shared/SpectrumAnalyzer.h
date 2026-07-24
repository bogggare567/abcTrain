#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>

// Generic live-spectrum display, used directly by LearnerComp/LearnerVerb
// (a plain input spectrum, no overlay) and as a base class by LearnerEQ's
// SpectrumAnalyserComponent (LearnerEQ/Source/SpectrumAnalyser.h), which
// layers a response-curve + highlighted-band overlay on top via
// paintOverlay(). This is the same extraction shared/WaveformDisplay.h
// already went through: it started as LearnerEQ-only, and once a second
// and third consumer needed the identical FIFO-accumulate/30 Hz-timer/FFT
// shape, it was pulled out here rather than copied - see
// decisions/006-unified-visualization.md.
//
// Deliberately knows nothing about EQ bands, filter coefficients, or
// highlighting - that stays in LearnerEQ/Source/SpectrumAnalyser.h.
class SpectrumAnalyzerComponent : public juce::Component,
                                   private juce::Timer
{
public:
    SpectrumAnalyzerComponent();

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    void pushNextSampleIntoFifo (float sample) noexcept;

    // Message thread only - keeps the frequency axis accurate. A plugin
    // that never calls this still gets a plausible-looking axis (44.1 kHz
    // default) rather than garbage.
    void setSampleRate (double newSampleRate) noexcept { sampleRate = newSampleRate; }

    void paint (juce::Graphics&) override;

protected:
    // Hook for a subclass that wants to draw something on top of the
    // spectrum, in the same bounds. Default does nothing - LearnerComp and
    // LearnerVerb use this class as-is, with no overlay.
    virtual void paintOverlay (juce::Graphics&, juce::Rectangle<float>) {}

private:
    void timerCallback() override;
    void drawNextFrameOfSpectrum();

    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static float proportionToFrequency (float proportion) noexcept;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, (size_t) fftSize> fifo {};
    std::array<float, (size_t) fftSize * 2> fftData {};
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;

    static constexpr int scopeSize = 512;
    std::array<float, (size_t) scopeSize> scopeData {};

    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzerComponent)
};

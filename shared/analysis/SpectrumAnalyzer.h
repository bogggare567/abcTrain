#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>
#include <cmath>

// Generic live-spectrum display, used directly by the trainer's hint and
// as a base class by LearnerEQ's SpectrumAnalyserComponent
// (LearnerEQ/Source/SpectrumAnalyser.h), which layers a response curve and
// highlighted band on top via paintOverlay(). See decisions/006 and 038.
//
// Threading (ADR 038). The audio thread only ever writes samples into a
// lock-free single-producer/single-consumer FIFO (juce::AbstractFifo) -
// no flag shared with the message thread, no copy the timer might be
// reading at the same moment. The first version handed a whole FFT block
// across with a plain bool, which is a data race: undefined behaviour in
// C++, however harmless it looked on one machine.
//
// What makes it look smooth:
//   - a 4096-point FFT recomputed on every 60 Hz frame over the latest
//     samples, so consecutive frames overlap by ~95% rather than jumping
//     from one block to the next;
//   - every display point takes the loudest bin across the band of
//     frequencies it covers (a single bin per point aliased the top
//     octaves into a comb) and interpolates between bins where the point
//     is narrower than a bin (the bottom octaves were a staircase);
//   - a +3 dB/octave tilt about 1 kHz, the usual analyser convention, so
//     pink noise and a balanced mix draw level instead of as a slope;
//   - time-based ballistics - fast attack, slow release - and a thin peak
//     line that holds and then falls, so what was loud a moment ago is
//     still readable.
class SpectrumAnalyzerComponent : public juce::Component,
                                   private juce::Timer
{
public:
    SpectrumAnalyzerComponent();

    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;

    // Audio thread. Real-time safe: a store into a local batch, and every
    // 64 samples one lock-free FIFO write. Never allocates, never blocks;
    // if the display falls behind, samples are dropped, not waited for.
    void pushNextSampleIntoFifo (float sample) noexcept;

    // Message thread only - keeps the frequency axis accurate. A plugin
    // that never calls this still gets a plausible-looking axis (44.1 kHz
    // default) rather than garbage.
    void setSampleRate (double newSampleRate) noexcept { sampleRate = newSampleRate; }

    // Per-instance accent, so a host with two Learner plugins open shows
    // each in its own family colour. Transparent means the palette's.
    void setAccentColour (juce::Colour newAccent) { accentOverride = newAccent; repaint(); }


    // Drawn straight onto the page, without its own well: for the trainer's
    // hint, which sits on the page background (possibly the player's own
    // picture) instead of in a box of its own.
    void setBackdropVisible (bool shouldShow) { backdrop = shouldShow; setOpaque (shouldShow && opaqueWithBackdrop); repaint(); }
    void paint (juce::Graphics&) override;

    // Tilt applied to the display, in dB per octave about 1 kHz.
    static constexpr float tiltDbPerOctave = 3.0f;
    static constexpr float floorDb = -90.0f;
    static constexpr float ceilingDb = 0.0f;

    // For tests: the displayed level (0..1) at a frequency, after
    // smoothing. Message thread.
    float getDisplayedLevelAt (float frequency) const noexcept;

    // For tests and snapshots: pull whatever the FIFO holds and run the
    // analysis now, with `seconds` of ballistics applied at once.
    void processPendingForTest (double seconds) { analyse (seconds); }

protected:
    juce::Colour effectiveAccent() const;

    // Hook for a subclass that wants to draw on top of the spectrum, in the
    // same bounds.
    virtual void paintOverlay (juce::Graphics&, juce::Rectangle<float>) {}

private:
    void timerCallback() override;
    void analyse (double elapsedSeconds);

    juce::Colour accentOverride { juce::Colours::transparentBlack };

    juce::Path buildSpectrumPath (const std::array<float, 512>& levels, juce::Rectangle<float> bounds) const;
    void paintGrid (juce::Graphics&, juce::Rectangle<float> bounds) const;

    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static float proportionToFrequency (float proportion) noexcept;
    static float frequencyToProportion (float frequency) noexcept;

    // Audio -> message thread.
    static constexpr int fifoCapacity = 1 << 15;
    static constexpr int batchSize = 64;
    juce::AbstractFifo fifo { fifoCapacity };
    std::array<float, (size_t) fifoCapacity> fifoStorage {};
    std::array<float, (size_t) batchSize> batch {};   // audio thread only
    int batchCount = 0;                               // audio thread only

    // Message thread only from here down.
    std::array<float, (size_t) fftSize> history {};
    int historyWrite = 0;
    int pendingNew = 0;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, (size_t) fftSize * 2> fftData {};

    static constexpr int scopeSize = 512;
    std::array<float, (size_t) scopeSize> target {};
    std::array<float, (size_t) scopeSize> smoothed {};
    std::array<float, (size_t) scopeSize> peak {};
    std::array<float, (size_t) scopeSize> peakHold {};   // seconds left before the peak falls

    double sampleRate = 44100.0;
    double lastTick = 0.0;
    bool idle = true;

    bool backdrop = true;
    bool opaqueWithBackdrop = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzerComponent)
};

#pragma once

#include "../../shared/SpectrumAnalyzer.h"
#include <array>

// EQ-specific view: the live spectrum comes from shared/SpectrumAnalyzer
// (see decisions/006-unified-visualization.md for why it was extracted);
// this class only adds the combined 4-band response curve and a
// highlighted-band overlay, drawn on top via paintOverlay(). Kept
// EQ-specific because EQCoefficients and the highlighted-band concept
// only make sense for LearnerEQ - the shared base class knows nothing
// about either.
class SpectrumAnalyserComponent : public SpectrumAnalyzerComponent
{
public:
    // Explicit and defaulted: JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR
    // below declares a deleted copy constructor, and a class with *any*
    // user-declared constructor (deleted or not) gets no implicit default
    // constructor - this class needs one since all the real construction
    // work now happens in the base class (shared/SpectrumAnalyzer.h).
    SpectrumAnalyserComponent() = default;

    // Called from the message thread once per frame with the current
    // parameter values, so the response curve tracks knob movement even
    // when no audio is playing.
    void setEQState (double sampleRate, const std::array<float, 4>& freqs,
                      const std::array<float, 4>& gains, const std::array<float, 4>& qs);

    void setHighlightedBand (int bandIndex) noexcept;

protected:
    void paintOverlay (juce::Graphics&, juce::Rectangle<float> bounds) override;

private:
    juce::Path buildResponseCurvePath (juce::Rectangle<float> bounds) const;

    double eqSampleRate = 44100.0;
    std::array<float, 4> eqFreqs { 100.0f, 800.0f, 3000.0f, 8000.0f };
    std::array<float, 4> eqGains {};
    std::array<float, 4> eqQs { 0.7f, 0.7f, 0.7f, 0.7f };
    int highlightedBandIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyserComponent)
};

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Arc-style gain-reduction meter for LearnerComp: a gradient track that
// fills from the top of the arc *downward* as the compressor pulls the
// signal down, glowing more strongly the harder it's working.
//
// Downward-filling is the point. A compressor's gain reduction is the one
// meter in a mixing chain that reads "more is lower", and drawing it as a
// bar that grows upward - which is what a reused level meter would do -
// silently teaches the wrong mental model to exactly the audience this
// plugin exists to teach.
//
// Fed from the message thread by the editor's existing 30 Hz timer
// (setGainReductionDb), not from the audio thread. The displayed value
// eases toward the target on the component's own timer so the needle has
// weight instead of snapping between frames: fast to react when reduction
// starts, slower to recover, which mirrors what the ear hears anyway.
class GainReductionMeter : public juce::Component,
                            private juce::Timer
{
public:
    GainReductionMeter();
    ~GainReductionMeter() override;

    // Positive dB of reduction (0 = not compressing at all).
    void setGainReductionDb (float newValue) noexcept { targetDb = newValue; }

    // Full-scale of the arc. 24 dB matches WaveformDisplay::highlightRangeDb
    // so the waveform tint and this meter always agree about what "fully
    // compressing" looks like.
    static constexpr float rangeDb = 24.0f;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    float targetDb = 0.0f;
    float displayedDb = 0.0f;
    float glow = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainReductionMeter)
};

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>

// Lissajous goniometer: plots left against right, rotated 45 degrees so
// mono sits on the vertical axis and out-of-phase content spreads
// horizontally - the orientation every hardware vectorscope uses, and the
// one an engineer reads without thinking.
//
// What it shows, and why it's the right hint for the stereo exercises:
//  - a vertical line     = mono, dead centre
//  - a tilted line       = panned, and which way it leans is which side
//  - a widening blob     = a real stereo image
//  - a horizontal spread = out of phase, the thing that vanishes in mono
//
// Fed from the audio thread by pushSample(); drawn from a decaying trail
// so the shape persists long enough to read rather than flickering one
// frame at a time. The FIFO is a plain ring buffer read without a lock -
// the only race is "a repaint sees a partially-updated trail", which is
// visually harmless for a scope, the same reasoning shared/WaveformDisplay
// and shared/SpectrumAnalyzer already use.
class Vectorscope : public juce::Component,
                     private juce::Timer
{
public:
    Vectorscope();
    ~Vectorscope() override;

    void pushSample (float left, float right) noexcept;

    // Clears the trail - call when the round changes, so the previous
    // round's image can't linger into the next one.
    void reset() noexcept;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    static constexpr int numPoints = 2048;

    std::array<std::atomic<float>, numPoints> pointsX {};
    std::array<std::atomic<float>, numPoints> pointsY {};
    std::atomic<int> writeIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vectorscope)
};

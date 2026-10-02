#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

// The audiometry test's stimulus (ADR 051): after a pause, three 250 ms
// pulses of a pure tone with 250 ms between them, in one ear only.
//
// Each pulse rises and falls over 20 ms on a raised cosine. A sine
// switched on or off abruptly splatters energy across the spectrum - a
// click - and a click at a level the tone itself is inaudible at is heard,
// which would measure the click, not the frequency. ISO 8253-1 asks for
// rise and fall times of 20 to 50 ms for the same reason.
//
// The level is the sine's peak in dBFS at the app's output. Three pulses
// rather than one steady tone: pulsed tones are easier to tell from
// tinnitus and from the background, and the BSA procedure allows them.
//
// Threading: present() and stop() are called from the message thread and
// only write atomics; render() runs on the audio thread and never
// allocates or waits.
class ProbeTone
{
public:
    static constexpr double pulseSeconds = 0.25;
    static constexpr double gapSeconds = 0.25;
    static constexpr double rampSeconds = 0.02;
    static constexpr int numPulses = 3;

    // From the end of the pause to the end of the last pulse.
    static constexpr double stimulusSeconds = numPulses * pulseSeconds + (numPulses - 1) * gapSeconds;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    }

    // One presentation. `channel` 0 is left, 1 right. `silent` plays
    // nothing for the same length of time - a catch trial.
    void present (int channelIndex, float freqHz, float levelDb, double delaySeconds, bool silent) noexcept
    {
        requestChannel.store (channelIndex, std::memory_order_relaxed);
        requestFreq.store (freqHz, std::memory_order_relaxed);
        requestGain.store (silent ? 0.0f : juce::Decibels::decibelsToGain (levelDb, -200.0f), std::memory_order_relaxed);
        requestDelay.store ((float) delaySeconds, std::memory_order_relaxed);
        requests.fetch_add (1, std::memory_order_release);
    }

    void stop() noexcept
    {
        stops.fetch_add (1, std::memory_order_release);
    }

    // Audio thread: replaces the buffer's contents with the probe.
    void render (juce::AudioBuffer<float>& buffer) noexcept
    {
        buffer.clear();

        if (const auto s = stops.load (std::memory_order_acquire); s != seenStops)
        {
            seenStops = s;
            playing = false;
        }

        if (const auto r = requests.load (std::memory_order_acquire); r != seenRequests)
        {
            seenRequests = r;
            channel = requestChannel.load (std::memory_order_relaxed);
            gain = requestGain.load (std::memory_order_relaxed);
            phaseStep = juce::MathConstants<double>::twoPi * (double) requestFreq.load (std::memory_order_relaxed) / sampleRate;
            delaySamples = (juce::int64) std::llround ((double) requestDelay.load (std::memory_order_relaxed) * sampleRate);
            position = 0;
            phase = 0.0;
            playing = true;
        }

        if (! playing)
            return;

        const auto pulse = (juce::int64) std::llround (pulseSeconds * sampleRate);
        const auto period = (juce::int64) std::llround ((pulseSeconds + gapSeconds) * sampleRate);
        const auto ramp = juce::jmax ((juce::int64) 1, (juce::int64) std::llround (rampSeconds * sampleRate));
        const auto end = delaySamples + (numPulses - 1) * period + pulse;
        const auto target = channel < buffer.getNumChannels() ? channel : buffer.getNumChannels() - 1;

        if (target < 0)
            return;

        auto* out = buffer.getWritePointer (target);

        for (int i = 0; i < buffer.getNumSamples(); ++i, ++position)
        {
            if (position >= end)
            {
                playing = false;
                break;
            }

            const auto t = position - delaySamples;

            if (t < 0)
                continue;

            const auto inPulse = t % period;

            if (inPulse >= pulse)
                continue;

            auto envelope = 1.0;

            if (inPulse < ramp)
                envelope = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * (double) inPulse / (double) ramp);
            else if (inPulse >= pulse - ramp)
                envelope = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * (double) (pulse - inPulse) / (double) ramp);

            out[i] = (float) (gain * envelope * std::sin (phase + phaseStep * (double) t));
        }
    }

    bool isPlaying() const noexcept { return playing; }

private:
    double sampleRate = 44100.0;

    std::atomic<int> requestChannel { 0 };
    std::atomic<float> requestFreq { 1000.0f }, requestGain { 0.0f }, requestDelay { 0.0f };
    std::atomic<int> requests { 0 }, stops { 0 };

    // Audio thread only.
    int seenRequests = 0, seenStops = 0;
    int channel = 0;
    float gain = 0.0f;
    double phase = 0.0, phaseStep = 0.0;
    juce::int64 delaySamples = 0, position = 0;
    bool playing = false;
};

#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>

// How loud the output is, the way a sound level meter set to "A" would say
// it - but in dBFS, because a plugin cannot know what the speakers do with
// its numbers. The conversion to dB SPL is one offset, measured once by
// the player (see HearingGuard and ADR 036).
//
// The A curve is built from its analogue definition (IEC 61672): four
// zeros at DC and poles at 20.6 Hz (twice), 107.7 Hz, 737.9 Hz and
// 12194 Hz (twice), each pole a first-order section through the bilinear
// transform, normalised to 0 dB at 1 kHz. Near Nyquist at 44.1 kHz the
// top poles warp a little; the error there is a fraction of a dB on
// material with almost no energy that high, which is far inside what a
// phone measurement can resolve anyway.
//
// Threading: process() is called on the audio thread and only adds to two
// atomics; drain() is called from a timer on the message thread and takes
// what has accumulated since the last call. No locks, no allocation.
class AWeightedMeter
{
public:
    void prepare (double newSampleRate, int numChannels)
    {
        sampleRate = newSampleRate;
        channels = juce::jlimit (1, maxChannels, numChannels);

        for (auto& chain : chains)
            chain = makeChain (sampleRate);

        gain = 1.0f / magnitudeAt (1000.0, sampleRate);

        for (auto& chain : chains)
            for (auto& f : chain)
                f.reset();
    }

    // Adds this block's A-weighted energy. `buffer` is read, not changed.
    void process (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto n = buffer.getNumSamples();
        const auto chs = juce::jmin (channels, buffer.getNumChannels());

        if (n == 0 || chs == 0)
            return;

        double sum = 0.0;

        for (int ch = 0; ch < chs; ++ch)
        {
            auto& chain = chains[(size_t) ch];
            const auto* in = buffer.getReadPointer (ch);

            for (int i = 0; i < n; ++i)
            {
                auto x = in[i];

                for (auto& f : chain)
                    x = f.processSample (x);

                x *= gain;
                sum += (double) x * (double) x;
            }
        }

        // Mean across channels, so stereo and mono report the same number
        // for the same signal in each channel.
        add (energy, sum / (double) chs);
        add (samples, (double) n);
    }

    // Mean square since the last drain, in full-scale units (a full-scale
    // square wave at 1 kHz reads 1.0), and how many seconds that covered.
    // Only ratios of these matter: HearingGuard compares every reading
    // with the reading of the calibration noise.
    struct Reading { double meanSquare = 0.0; double seconds = 0.0; };

    Reading drain() noexcept
    {
        const auto e = energy.exchange (0.0);
        const auto s = samples.exchange (0.0);

        if (s <= 0.0 || sampleRate <= 0.0)
            return {};

        return { e / s, s / sampleRate };
    }

    // The same weighting applied offline to a buffer, for measuring a
    // reference signal once (the calibration noise).
    static double meanSquareOf (const juce::AudioBuffer<float>& buffer, double sr)
    {
        AWeightedMeter meter;
        meter.prepare (sr, buffer.getNumChannels());
        meter.process (buffer);
        return meter.drain().meanSquare;
    }

    static float magnitudeAt (double frequency, double sr)
    {
        auto chain = makeChain (sr);
        double m = 1.0;

        for (auto& f : chain)
            m *= f.coefficients->getMagnitudeForFrequency (frequency, sr);

        return (float) m;
    }

    // dB relative to 1 kHz, for tests against the standard's table.
    static float responseDb (double frequency, double sr)
    {
        return juce::Decibels::gainToDecibels (magnitudeAt (frequency, sr) / magnitudeAt (1000.0, sr), -200.0f);
    }

private:
    static constexpr int maxChannels = 2;
    static constexpr int sections = 6;

    using Filter = juce::dsp::IIR::Filter<float>;
    using Chain = std::array<Filter, sections>;

    static Chain makeChain (double sr)
    {
        using C = juce::dsp::IIR::Coefficients<float>;
        const auto nyquistSafe = [sr] (double f) { return (float) juce::jmin (f, sr * 0.45); };

        Chain chain;
        chain[0].coefficients = C::makeFirstOrderHighPass (sr, 20.6f);
        chain[1].coefficients = C::makeFirstOrderHighPass (sr, 20.6f);
        chain[2].coefficients = C::makeFirstOrderHighPass (sr, 107.7f);
        chain[3].coefficients = C::makeFirstOrderHighPass (sr, 737.9f);
        chain[4].coefficients = C::makeFirstOrderLowPass (sr, nyquistSafe (12194.0));
        chain[5].coefficients = C::makeFirstOrderLowPass (sr, nyquistSafe (12194.0));
        return chain;
    }

    static void add (std::atomic<double>& target, double amount) noexcept
    {
        auto current = target.load();
        while (! target.compare_exchange_weak (current, current + amount)) {}
    }

    double sampleRate = 0.0;
    int channels = 1;
    float gain = 1.0f;
    std::array<Chain, maxChannels> chains;

    std::atomic<double> energy { 0.0 }, samples { 0.0 };
};

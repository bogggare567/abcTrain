#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>
#include <utility>

// Making "before" and "after" the same loudness.
//
// Every A/B exercise hides one change and asks you to hear it. If the
// processed side is louder, there is a second change - and loudness is
// the easiest difference there is, so the round stops being about
// reverb or width or a frequency and becomes "which one is louder". The
// player learns to answer correctly by hearing the wrong thing, which is
// worse than not learning at all.
//
// Several exercises leak level by construction and none of them meant to:
// a peak filter boosting 10 dB raises the total, a reverb adds its own
// energy on top of the dry signal, a delay adds repeats, and widening the
// side signal raises the sum of the two channels. CompressionGame and
// DistortionGame already solved this by *measuring* rather than guessing;
// this is the same idea as a helper the rest can share.
//
// Deliberately RMS rather than peak: this is about perceived loudness over
// a repeating signal, and a peak match would be thrown by a single
// transient the processing happened to sharpen.
namespace GainMatch
{
    inline float rms (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto numChannels = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();

        if (numChannels <= 0 || numSamples <= 0)
            return 0.0f;

        auto sum = 0.0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
                sum += (double) data[i] * (double) data[i];
        }

        return (float) std::sqrt (sum / (double) (numChannels * numSamples));
    }

    // How "the same loudness" is measured (t03, ADR 055). Plain RMS, or
    // ITU-R BS.1770 K-weighting: a +4 dB shelf above ~1.7 kHz and a
    // high-pass at 38 Hz before the mean square - the weighting LUFS
    // meters use, closer to how loud a change *sounds* when it moves the
    // top or the very bottom. No gating: what is measured is a short loop
    // that never falls silent, where the gate never closes.
    enum class Mode { rms = 0, bs1770 = 1 };
    inline std::atomic<int> mode { (int) Mode::rms };
    inline std::atomic<double> sampleRate { 44100.0 };

    // The two K-weighting stages for a sample rate (BS.1770-4, the
    // analogue prototypes evaluated per rate, as libebur128 does).
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;
        double process (double x) noexcept
        {
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    inline std::pair<Biquad, Biquad> kWeighting (double fs) noexcept
    {
        Biquad shelf, hp;
        {
            const auto f0 = 1681.974450955533, g = 3.999843853973347, q = 0.7071752369554196;
            const auto k = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const auto vh = std::pow (10.0, g / 20.0);
            const auto vb = std::pow (vh, 0.4996667741545416);
            const auto a0 = 1.0 + k / q + k * k;
            shelf.b0 = (vh + vb * k / q + k * k) / a0;
            shelf.b1 = 2.0 * (k * k - vh) / a0;
            shelf.b2 = (vh - vb * k / q + k * k) / a0;
            shelf.a1 = 2.0 * (k * k - 1.0) / a0;
            shelf.a2 = (1.0 - k / q + k * k) / a0;
        }
        {
            const auto f0 = 38.13547087602444, q = 0.5003270373238773;
            const auto k = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const auto a0 = 1.0 + k / q + k * k;
            hp.b0 = 1.0; hp.b1 = -2.0; hp.b2 = 1.0;
            hp.a1 = 2.0 * (k * k - 1.0) / a0;
            hp.a2 = (1.0 - k / q + k * k) / a0;
        }
        return { shelf, hp };
    }

    // Root of the K-weighted mean square, summed over channels the way
    // BS.1770 sums them (each channel weighted 1 for L/R).
    inline float kRms (const juce::AudioBuffer<float>& buffer, double fs) noexcept
    {
        const auto numChannels = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();
        if (numChannels <= 0 || numSamples <= 0)
            return 0.0f;

        auto sum = 0.0;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto [shelf, hp] = kWeighting (fs);
            const auto* data = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const auto y = hp.process (shelf.process ((double) data[i]));
                sum += y * y;
            }
        }
        return (float) std::sqrt (sum / (double) (numChannels * numSamples));
    }

    // The loudness the matching compares, in whichever mode is chosen.
    inline float level (const juce::AudioBuffer<float>& buffer) noexcept
    {
        return mode.load() == (int) Mode::bs1770 ? kRms (buffer, sampleRate.load()) : rms (buffer);
    }

    // The gain that brings `wet` back to `dry`.
    //
    // Clamped, and 1 for anything degenerate: a silent wet path would
    // otherwise ask for infinite gain, and a NaN reaching the audio
    // thread is a much worse bug than an unmatched level.
    inline float from (float dryRms, float wetRms) noexcept
    {
        if (! (dryRms > 1.0e-6f) || ! (wetRms > 1.0e-6f))
            return 1.0f;

        return juce::jlimit (0.05f, 20.0f, dryRms / wetRms);
    }

    // Measures one round's processing offline and returns the gain that
    // levels it against the untreated signal.
    //
    // `renderDry` fills a buffer with the signal the exercise plays with
    // nothing applied; `applyWet` processes a copy of it. Both run on the
    // message thread, in newRound(), so the audio thread only ever reads
    // the resulting float.
    // `warmUpSamples` are processed but not measured. An effect with a
    // tail - a reverb, a delay - starts from silence here and takes a
    // while to reach the steady state the player actually hears, so
    // measuring through that start reports the wet path as quieter than
    // it is. The live one has been running for as long as the round has.
    // `numChannels` must match what the live path processes. It is not a
    // formality: juce::dsp::Reverb runs a different algorithm on one
    // channel than on two - the stereo one spreads its taps and applies a
    // width - so a mono measurement of a stereo effect measures a
    // different effect and lands several dB out.
    template <typename RenderDry, typename ApplyWet>
    float measure (int numChannels, int numSamples, int warmUpSamples,
                   RenderDry&& renderDry, ApplyWet&& applyWet)
    {
        if (numSamples <= 0 || numChannels <= 0)
            return 1.0f;

        const auto skip = juce::jlimit (0, numSamples - 1, warmUpSamples);
        const auto measured = numSamples - skip;

        juce::AudioBuffer<float> dry (numChannels, numSamples);
        dry.clear();
        renderDry (dry);

        juce::AudioBuffer<float> wet (numChannels, numSamples);
        wet.makeCopyOf (dry);
        applyWet (wet);

        juce::AudioBuffer<float> dryTail (numChannels, measured);
        juce::AudioBuffer<float> wetTail (numChannels, measured);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            dryTail.copyFrom (ch, 0, dry, ch, skip, measured);
            wetTail.copyFrom (ch, 0, wet, ch, skip, measured);
        }

        return from (level (dryTail), level (wetTail));
    }

    template <typename RenderDry, typename ApplyWet>
    float measure (int numSamples, RenderDry&& renderDry, ApplyWet&& applyWet)
    {
        return measure (1, numSamples, 0,
                        std::forward<RenderDry> (renderDry),
                        std::forward<ApplyWet> (applyWet));
    }
}

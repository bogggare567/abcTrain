#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
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

        return from (rms (dryTail), rms (wetTail));
    }

    template <typename RenderDry, typename ApplyWet>
    float measure (int numSamples, RenderDry&& renderDry, ApplyWet&& applyWet)
    {
        return measure (1, numSamples, 0,
                        std::forward<RenderDry> (renderDry),
                        std::forward<ApplyWet> (applyWet));
    }
}

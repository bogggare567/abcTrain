#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// Custom feed-forward compressor with a soft-knee gain computer (the
// standard formula from Giannoulis/Massberg/Reiss, "Digital Dynamic
// Range Compressor Design") and one-pole attack/release smoothing on the
// gain-reduction envelope. Written from scratch instead of using
// juce::dsp::Compressor because that class has neither knee control nor
// a way to read back its internal gain reduction - both of which
// LearnerComp's teaching visualization needs.
class CompressorEngine
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    }

    void reset() noexcept { envelopeDb = 0.0f; }

    void setParameters (float newThresholdDb, float newRatio, float newAttackMs,
                         float newReleaseMs, float newKneeDb, float newMakeupGainDb) noexcept
    {
        thresholdDb = newThresholdDb;
        ratio = juce::jmax (1.0f, newRatio);
        kneeDb = juce::jmax (0.0f, newKneeDb);
        makeupGainDb = newMakeupGainDb;

        attackCoeff = timeToCoefficient (newAttackMs);
        releaseCoeff = timeToCoefficient (newReleaseMs);
    }

    // Updates the smoothed gain-reduction envelope from one detection
    // sample (e.g. the loudest of the input channels at this sample, for
    // stereo-linked detection) and returns the linear gain - compression
    // plus makeup - to apply to the actual audio sample(s).
    float computeGain (float detectionSample) noexcept
    {
        const auto inputDb = juce::Decibels::gainToDecibels (std::abs (detectionSample), -100.0f);
        const auto overDb = inputDb - thresholdDb;

        float targetReductionDb;
        if (kneeDb <= 0.0f)
        {
            targetReductionDb = overDb > 0.0f ? overDb * (1.0f - 1.0f / ratio) : 0.0f;
        }
        else if (2.0f * overDb < -kneeDb)
        {
            targetReductionDb = 0.0f;
        }
        else if (2.0f * std::abs (overDb) <= kneeDb)
        {
            const auto x = overDb + kneeDb * 0.5f;
            targetReductionDb = (1.0f - 1.0f / ratio) * (x * x) / (2.0f * kneeDb);
        }
        else
        {
            targetReductionDb = overDb * (1.0f - 1.0f / ratio);
        }

        const auto coeff = (targetReductionDb > envelopeDb) ? attackCoeff : releaseCoeff;
        envelopeDb = coeff * envelopeDb + (1.0f - coeff) * targetReductionDb;

        return juce::Decibels::decibelsToGain (makeupGainDb - envelopeDb);
    }

    float getLastGainReductionDb() const noexcept { return envelopeDb; }

private:
    float timeToCoefficient (float timeMs) const noexcept
    {
        const auto timeSeconds = juce::jmax (0.0001f, timeMs * 0.001f);
        return std::exp (-1.0f / (timeSeconds * (float) sampleRate));
    }

    double sampleRate = 44100.0;

    float thresholdDb = -12.0f;
    float ratio = 4.0f;
    float kneeDb = 0.0f;
    float makeupGainDb = 0.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    float envelopeDb = 0.0f;
};

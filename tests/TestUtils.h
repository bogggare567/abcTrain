#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace TestUtils
{
    inline juce::AudioBuffer<float> generateSineBuffer (float freqHz, double sampleRate, int numSamples,
                                                          int numChannels = 1, float amplitude = 0.5f)
    {
        juce::AudioBuffer<float> buffer (numChannels, numSamples);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto value = amplitude
                              * std::sin (2.0 * juce::MathConstants<double>::pi * (double) freqHz
                                          * (double) sample / sampleRate);

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, sample, (float) value);
        }

        return buffer;
    }

    inline float rms (const juce::AudioBuffer<float>& buffer)
    {
        double sumSquares = 0.0;
        juce::int64 count = 0;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                sumSquares += (double) data[i] * (double) data[i];
                ++count;
            }
        }

        return count > 0 ? (float) std::sqrt (sumSquares / (double) count) : 0.0f;
    }
}

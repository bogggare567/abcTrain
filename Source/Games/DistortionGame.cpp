#include "DistortionGame.h"
#include <cmath>

const std::array<DistortionGame::TypeInfo, DistortionGame::numTypes> DistortionGame::types {{
    { "Soft Clipping",   1.0f  },
    { "Hard Clipping",   0.85f },
    { "Tape Saturation", 1.3f  },
    { "Overdrive",       1.0f  }
}};

void DistortionGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // One-pole lowpass coefficient for the Tape Saturation type's post-
    // clip rolloff, cutoff ~4 kHz - gives it a "warmer," less bright
    // top end than the other three types even at the same drive amount.
    constexpr float cutoffHz = 4000.0f;
    tapeLowpassCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * cutoffHz / (float) sampleRate);

    newRound();
}

float DistortionGame::waveshape (Type type, float driven) const
{
    switch (type)
    {
        case Type::softClip:
            return std::tanh (driven);

        case Type::hardClip:
            return juce::jlimit (-1.0f, 1.0f, driven);

        case Type::tapeSaturation:
            return std::tanh (driven); // lowpass applied afterward in process()

        case Type::overdrive:
            // Asymmetric knee (softer on the negative half) gives
            // overdrive its characteristic even-harmonic-rich sound,
            // unlike Soft Clipping's symmetric tanh.
            return driven >= 0.0f ? std::tanh (driven) : std::tanh (driven * 0.6f);
    }

    return driven;
}

void DistortionGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const auto type = (Type) correctTypeIndex;
    const auto makeup = types[(size_t) correctTypeIndex].makeupGain;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto driven = noise.nextSample() * driveAmount;
        auto shaped = waveshape (type, driven);

        if (type == Type::tapeSaturation)
        {
            tapeLowpassState += tapeLowpassCoeff * (shaped - tapeLowpassState);
            shaped = tapeLowpassState;
        }

        const auto value = shaped * makeup * 0.35f;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);
    }
}

void DistortionGame::setDifficulty (int level)
{
    if (level <= 3)
        driveAmount = 6.0f;
    else if (level <= 6)
        driveAmount = 3.0f;
    else
        driveAmount = 1.5f;
}

void DistortionGame::newRound()
{
    correctTypeIndex = random.nextInt (numTypes);
    chosenTypeIndex = -1;
    answered = false;
    tapeLowpassState = 0.0f;
    sendChangeMessage();
}

void DistortionGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenTypeIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctTypeIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String DistortionGame::getChoiceLabel (int choiceIndex) const
{
    return types[(size_t) choiceIndex].label;
}

juce::String DistortionGame::getFeedbackText() const
{
    if (! answered)
        return {};

    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + types[(size_t) correctTypeIndex].label + ".";
}

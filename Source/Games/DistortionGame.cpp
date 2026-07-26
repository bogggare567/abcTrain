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

    const auto processed = playProcessed.load();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto raw = noise.nextSample();
        const auto driven = raw * juce::jmax (0.2f, driveAmount + roundDriveJitter);
        auto shaped = waveshape (type, driven);

        if (type == Type::tapeSaturation)
        {
            tapeLowpassState += tapeLowpassCoeff * (shaped - tapeLowpassState);
            shaped = tapeLowpassState;
        }

        // "Clean" keeps the same makeup gain, so switching A/B isolates
        // the harmonic character rather than the level.
        const auto value = (processed ? shaped : raw) * makeup * 0.35f;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);
    }
}

void DistortionGame::setDifficulty (int level)
{
    // Ramped across all ten levels, like every other game: the drive falls
    // from an obvious 6x to a subtle 1.2x, where the four types have to be
    // told apart by the harmonics they add rather than by how loud the
    // distortion is.
    driveAmount = rampTolerance (level, 6.0f, 1.2f);
}

void DistortionGame::newRound()
{
    correctTypeIndex = random.nextInt (numTypes);

    // A per-round nudge on the drive, for the same reason the other
    // preset-driven games have one: four fixed drive amounts become four
    // memorised recordings, and then the exercise is recognition rather
    // than listening. Scaled by the current drive so it shrinks as the
    // levels get harder and the margins get thinner.
    roundDriveJitter = (random.nextFloat() * 0.3f - 0.15f) * driveAmount;
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

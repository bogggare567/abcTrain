#include "CompressionGame.h"
#include <cmath>

const std::array<CompressionGame::Preset, CompressionGame::numLevels> CompressionGame::presets {{
    { "Weak",   -12.0f, 2.0f, 2.0f  },
    { "Medium", -18.0f, 4.0f, 6.0f  },
    { "Strong", -24.0f, 8.0f, 10.0f }
}};

void CompressionGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    compressor.prepare (spec);
    compressor.setAttack (5.0f);
    compressor.setRelease (120.0f);

    attackSamples = juce::jmax (1, (int) (sampleRate * 0.003));
    decayTauSamples = juce::jmax (1, (int) (sampleRate * 0.07));
    burstPeriodSamples = juce::jmax (1, (int) (sampleRate * 0.6));
    samplesSinceBurstStart = 0;

    newRound();
}

void CompressionGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (samplesSinceBurstStart >= burstPeriodSamples)
            samplesSinceBurstStart = 0;

        float envelope;
        if (samplesSinceBurstStart < attackSamples)
            envelope = (float) samplesSinceBurstStart / (float) attackSamples;
        else
            envelope = std::exp ((float) -(samplesSinceBurstStart - attackSamples) / (float) decayTauSamples);

        const auto value = noise.nextSample() * envelope;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);

        ++samplesSinceBurstStart;
    }

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    compressor.process (context);

    const auto& preset = presets[(size_t) correctLevelIndex];
    buffer.applyGain (juce::Decibels::decibelsToGain (preset.makeupGainDb) * 0.6f);
}

void CompressionGame::newRound()
{
    correctLevelIndex = random.nextInt (numLevels);
    chosenLevelIndex = -1;
    answered = false;
    updateCompressor();
    sendChangeMessage();
}

void CompressionGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenLevelIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctLevelIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String CompressionGame::getChoiceLabel (int choiceIndex) const
{
    return presets[(size_t) choiceIndex].label;
}

juce::String CompressionGame::getFeedbackText() const
{
    if (! answered)
        return {};

    const auto& preset = presets[(size_t) correctLevelIndex];
    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + preset.label + " compression ("
           + juce::String (preset.ratio, 0) + ":1 at "
           + juce::String (preset.thresholdDb, 0) + " dB).";
}

void CompressionGame::updateCompressor()
{
    const auto& preset = presets[(size_t) correctLevelIndex];
    compressor.setThreshold (preset.thresholdDb);
    compressor.setRatio (preset.ratio);
}

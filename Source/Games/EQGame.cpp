#include "EQGame.h"

namespace
{
    juce::String formatFrequency (float hz)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, 1) + "k";
        return juce::String ((int) hz);
    }
}

const std::array<float, EQGame::numBands> EQGame::bandFrequenciesHz {
    100.0f, 200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f, 6400.0f, 12800.0f
};

void EQGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    peakFilter.prepare (spec);
    newRound();
}

void EQGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto value = noise.nextSample();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);
    }

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    peakFilter.process (context);

    buffer.applyGain (0.25f);
}

void EQGame::newRound()
{
    correctBandIndex = random.nextInt (numBands);
    isBoost = random.nextBool();
    chosenBandIndex = -1;
    answered = false;
    updateFilter();
    sendChangeMessage();
}

void EQGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenBandIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctBandIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String EQGame::getChoiceLabel (int choiceIndex) const
{
    return formatFrequency (bandFrequenciesHz[(size_t) choiceIndex]) + " Hz";
}

juce::String EQGame::getFeedbackText() const
{
    if (! answered)
        return {};

    const juce::String direction = isBoost ? "boosted" : "cut";
    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + direction + " at "
           + formatFrequency (bandFrequenciesHz[(size_t) correctBandIndex]) + " Hz.";
}

void EQGame::updateFilter()
{
    const auto freq = bandFrequenciesHz[(size_t) correctBandIndex];
    const auto gain = juce::Decibels::decibelsToGain (isBoost ? gainDb : -gainDb);
    *peakFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, freq, filterQ, gain);
}

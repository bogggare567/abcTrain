#include "EQGame.h"

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
        const auto noise = nextPinkNoiseSample();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, noise);
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

void EQGame::submitAnswer (int bandIndex)
{
    if (answered)
        return;

    chosenBandIndex = bandIndex;
    lastAnswerCorrect = (bandIndex == correctBandIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

void EQGame::updateFilter()
{
    const auto freq = bandFrequenciesHz[(size_t) correctBandIndex];
    const auto gain = juce::Decibels::decibelsToGain (isBoost ? gainDb : -gainDb);
    *peakFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, freq, filterQ, gain);
}

float EQGame::nextPinkNoiseSample()
{
    // Paul Kellet's "economy" pink noise algorithm (musicdsp.org).
    const float white = random.nextFloat() * 2.0f - 1.0f;

    pinkState[0] = 0.99886f * pinkState[0] + white * 0.0555179f;
    pinkState[1] = 0.99332f * pinkState[1] + white * 0.0750759f;
    pinkState[2] = 0.96900f * pinkState[2] + white * 0.1538520f;
    pinkState[3] = 0.86650f * pinkState[3] + white * 0.3104856f;
    pinkState[4] = 0.55000f * pinkState[4] + white * 0.5329522f;
    pinkState[5] = -0.7616f * pinkState[5] - white * 0.0168980f;

    const float pink = pinkState[0] + pinkState[1] + pinkState[2] + pinkState[3]
                      + pinkState[4] + pinkState[5] + pinkState[6] + white * 0.5362f;
    pinkState[6] = white * 0.115926f;

    return pink * 0.11f;
}

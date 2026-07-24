#include "DelayGame.h"
#include <cmath>

const std::array<float, DelayGame::numDelayTimes> DelayGame::delayTimesMs { 50.0f, 150.0f, 300.0f, 500.0f };

void DelayGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    delayLine.setMaximumDelayInSamples ((int) (sampleRate * 0.6) + 16);
    delayLine.prepare (spec);

    attackSamples = juce::jmax (1, (int) (sampleRate * 0.003));
    decayTauSamples = juce::jmax (1, (int) (sampleRate * 0.07));
    updateBurstPeriod();
    samplesSinceBurstStart = 0;

    newRound();
}

void DelayGame::process (juce::AudioBuffer<float>& buffer)
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

        const auto dry = noise.nextSample() * envelope;

        delayLine.pushSample (0, dry);
        const auto wet = delayLine.popSample (0);
        const auto value = dryWetFraction * wet + (1.0f - dryWetFraction) * dry;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);

        ++samplesSinceBurstStart;
    }
}

void DelayGame::setDifficulty (int level)
{
    if (level <= 3)
        burstPeriodSeconds = 1.4f;
    else if (level <= 6)
        burstPeriodSeconds = 0.9f;
    else
        burstPeriodSeconds = 0.6f;

    updateBurstPeriod();
}

void DelayGame::updateBurstPeriod()
{
    burstPeriodSamples = juce::jmax (1, (int) (sampleRate * burstPeriodSeconds));
}

void DelayGame::newRound()
{
    correctDelayIndex = random.nextInt (numDelayTimes);
    chosenDelayIndex = -1;
    answered = false;
    updateDelayTime();
    sendChangeMessage();
}

void DelayGame::updateDelayTime()
{
    const auto ms = delayTimesMs[(size_t) correctDelayIndex];
    delayLine.setDelay ((float) (sampleRate * ms / 1000.0));
}

void DelayGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenDelayIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctDelayIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String DelayGame::getChoiceLabel (int choiceIndex) const
{
    return juce::String ((int) delayTimesMs[(size_t) choiceIndex]) + "ms";
}

juce::String DelayGame::getFeedbackText() const
{
    if (! answered)
        return {};

    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + juce::String ((int) delayTimesMs[(size_t) correctDelayIndex]) + "ms.";
}

#include "DBGame.h"

void DBGame::prepare (const juce::dsp::ProcessSpec&)
{
    newRound();
}

void DBGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    constexpr float baseGain = 0.4f;
    const auto deltaGain = juce::Decibels::decibelsToGain (getDeltaDb (correctChoiceIndex));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto value = noise.nextSample() * baseGain * deltaGain;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);
    }
}

void DBGame::setDifficulty (int level)
{
    if (level <= 3)
        stepDb = 6;
    else if (level <= 6)
        stepDb = 3;
    else
        stepDb = 2;
}

void DBGame::newRound()
{
    correctChoiceIndex = random.nextInt (numChoices);
    chosenChoiceIndex = -1;
    answered = false;
    sendChangeMessage();
}

void DBGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenChoiceIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctChoiceIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String DBGame::formatDb (int db)
{
    if (db == 0)
        return "0dB";
    return (db > 0 ? juce::String ("+") : juce::String()) + juce::String (db) + "dB";
}

juce::String DBGame::getChoiceLabel (int choiceIndex) const
{
    return formatDb ((int) getDeltaDb (choiceIndex));
}

juce::String DBGame::getFeedbackText() const
{
    if (! answered)
        return {};

    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + formatDb ((int) getDeltaDb (correctChoiceIndex)) + ".";
}

#include "DBGame.h"
#include <cmath>
#include <limits>

void DBGame::prepare (const juce::dsp::ProcessSpec&)
{
    newRound();
}

void DBGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    constexpr float baseGain = 0.4f;
    const auto deltaGain = juce::Decibels::decibelsToGain (targetDb);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto value = noise.nextSample() * baseGain * deltaGain;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);
    }
}

void DBGame::setDifficulty (int level)
{
    // stepDb still drives the legacy discrete labels; toleranceDb is what
    // difficulty means on the continuous scale.
    if (level <= 3)
    {
        stepDb = 6;
        toleranceDb = 2.5f;
    }
    else if (level <= 6)
    {
        stepDb = 3;
        toleranceDb = 1.5f;
    }
    else
    {
        stepDb = 2;
        toleranceDb = 1.0f;
    }
}

juce::String DBGame::formatNormalisedValue (float normalised) const
{
    const auto db = normalisedToDb (normalised);

    // One decimal, and an explicit + so a boost never reads as ambiguous.
    return (db >= 0.05f ? juce::String ("+") : juce::String())
               + juce::String (db, 1) + " dB";
}

std::vector<Game::GridMark> DBGame::getGridMarks() const
{
    std::vector<GridMark> marks;

    // Every dB, with the multiples of 3 emphasised - the numbers an
    // engineer actually thinks in.
    for (int db = (int) axisMinDb; db <= (int) axisMaxDb; ++db)
    {
        const auto normalised = dbToNormalised ((float) db);
        const auto emphasised = (db % 3 == 0);
        marks.push_back ({ normalised,
                           emphasised ? formatNormalisedValue (normalised) : juce::String(),
                           emphasised });
    }

    return marks;
}

void DBGame::submitNormalisedAnswer (float normalised)
{
    if (answered)
        return;

    chosenNormalised = juce::jlimit (0.0f, 1.0f, normalised);
    chosenChoiceIndex = -1;

    lastAnswerCorrect = std::abs (normalisedToDb (chosenNormalised) - targetDb) <= toleranceDb;

    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

void DBGame::newRound()
{
    // Any level change in the range, quantised to 0.5 dB so the answer is
    // always a value a person could plausibly name - a target of
    // -4.37 dB would be unfair however wide the tolerance.
    const auto steps = juce::roundToInt (axisSpanDb * 2.0f);
    targetDb = axisMinDb + (float) random.nextInt (steps + 1) * 0.5f;

    // Nearest legacy choice, for the discrete path only.
    correctChoiceIndex = 0;
    auto smallestDistance = std::numeric_limits<float>::max();
    for (int i = 0; i < numChoices; ++i)
    {
        const auto distance = std::abs (getDeltaDb (i) - targetDb);
        if (distance < smallestDistance)
        {
            smallestDistance = distance;
            correctChoiceIndex = i;
        }
    }

    chosenChoiceIndex = -1;
    chosenNormalised = -1.0f;
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
           + "It was " + formatNormalisedValue (dbToNormalised (targetDb)) + ".";
}

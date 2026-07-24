#include "PanGame.h"
#include <cmath>

const std::array<const char*, PanGame::numPositions> PanGame::positionLabels {
    "Hard Left", "Left", "Center", "Right", "Hard Right"
};

const std::array<float, PanGame::numPositions> PanGame::easyPositions {
    -1.0f, -0.5f, 0.0f, 0.5f, 1.0f
};

const std::array<float, PanGame::numPositions> PanGame::mediumPositions {
    -0.7f, -0.35f, 0.0f, 0.35f, 0.7f
};

const std::array<float, PanGame::numPositions> PanGame::hardPositions {
    -0.4f, -0.2f, 0.0f, 0.2f, 0.4f
};

void PanGame::prepare (const juce::dsp::ProcessSpec&)
{
    newRound();
}

void PanGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    const auto pan = (*activePositions)[(size_t) correctPositionIndex];
    // Equal-power pan law: theta sweeps 0..pi/2 as pan sweeps -1..+1, so
    // gainL^2 + gainR^2 stays constant (= total energy stays constant)
    // regardless of position - no separate loudness compensation needed.
    const auto theta = (pan + 1.0f) * (juce::MathConstants<float>::pi / 4.0f);
    const auto gainL = std::cos (theta);
    const auto gainR = std::sin (theta);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto value = noise.nextSample() * 0.5f;

        if (numChannels > 0) buffer.setSample (0, sample, value * gainL);
        if (numChannels > 1) buffer.setSample (1, sample, value * gainR);
        for (int ch = 2; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);
    }
}

void PanGame::setDifficulty (int level)
{
    if (level <= 3)
        activePositions = &easyPositions;
    else if (level <= 6)
        activePositions = &mediumPositions;
    else
        activePositions = &hardPositions;
}

void PanGame::newRound()
{
    correctPositionIndex = random.nextInt (numPositions);
    chosenPositionIndex = -1;
    answered = false;
    sendChangeMessage();
}

void PanGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenPositionIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctPositionIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String PanGame::getChoiceLabel (int choiceIndex) const
{
    return positionLabels[(size_t) choiceIndex];
}

juce::String PanGame::getFeedbackText() const
{
    if (! answered)
        return {};

    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was panned " + positionLabels[(size_t) correctPositionIndex] + ".";
}

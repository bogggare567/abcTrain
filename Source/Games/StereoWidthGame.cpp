#include "StereoWidthGame.h"

const std::array<const char*, StereoWidthGame::numWidths> StereoWidthGame::widthLabels {
    "Narrow", "Normal", "Wide", "Extra Wide"
};

const std::array<float, StereoWidthGame::numWidths> StereoWidthGame::easyWidths {
    0.1f, 0.6f, 1.0f, 1.6f
};

const std::array<float, StereoWidthGame::numWidths> StereoWidthGame::mediumWidths {
    0.3f, 0.7f, 1.0f, 1.3f
};

const std::array<float, StereoWidthGame::numWidths> StereoWidthGame::hardWidths {
    0.5f, 0.8f, 1.0f, 1.2f
};

void StereoWidthGame::prepare (const juce::dsp::ProcessSpec&)
{
    newRound();
}

void StereoWidthGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const auto width = (*activeWidths)[(size_t) correctWidthIndex];

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto left = noiseL.nextSample() * 0.5f;
        const auto right = noiseR.nextSample() * 0.5f;

        const auto mid = (left + right) * 0.5f;
        const auto side = (left - right) * 0.5f * width;

        const auto outL = mid + side;
        const auto outR = mid - side;

        if (numChannels > 0) buffer.setSample (0, sample, outL);
        if (numChannels > 1) buffer.setSample (1, sample, outR);
        for (int ch = 2; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, mid);
    }
}

void StereoWidthGame::setDifficulty (int level)
{
    if (level <= 3)
        activeWidths = &easyWidths;
    else if (level <= 6)
        activeWidths = &mediumWidths;
    else
        activeWidths = &hardWidths;
}

void StereoWidthGame::newRound()
{
    correctWidthIndex = random.nextInt (numWidths);
    chosenWidthIndex = -1;
    answered = false;
    sendChangeMessage();
}

void StereoWidthGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenWidthIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctWidthIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String StereoWidthGame::getChoiceLabel (int choiceIndex) const
{
    return widthLabels[(size_t) choiceIndex];
}

juce::String StereoWidthGame::getFeedbackText() const
{
    if (! answered)
        return {};

    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + widthLabels[(size_t) correctWidthIndex] + ".";
}

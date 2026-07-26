#include "StereoWidthGame.h"

const std::array<const char*, StereoWidthGame::numWidths> StereoWidthGame::widthLabels {
    "Narrow", "Normal", "Wide", "Extra Wide"
};

void StereoWidthGame::prepare (const juce::dsp::ProcessSpec&)
{
    newRound();
}

void StereoWidthGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const auto width = widths[(size_t) correctWidthIndex];

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
    // The four named widths are computed from one spread value rather than
    // picked from three hard-coded tables, so every level is a real step.
    // At level 1 they are 0.10 / 0.60 / 1.00 / 1.60 - unmistakable. At
    // level 10 they close to 0.62 / 0.86 / 1.00 / 1.14, which takes real
    // listening to separate.
    //
    // Everything is a distance from Normal (1.0), scaled by that spread,
    // and the ratios between the four are preserved - so "Wide" is always
    // wider than "Normal", at every level, which a table per tier could
    // only guarantee by hand.
    const auto spread = rampTolerance (level, 1.0f, 0.24f);

    const std::array<float, numWidths> offsets { -0.9f, -0.4f, 0.0f, 0.6f };

    for (size_t i = 0; i < widths.size(); ++i)
        widths[i] = 1.0f + offsets[i] * spread;
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

#include "PanGame.h"
#include <cmath>
#include <limits>

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

    const auto pan = playProcessed.load() ? targetPan : 0.0f;
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
    // The three position tables still drive the legacy discrete path; what
    // difficulty means for the continuous one is how wide the accept band
    // is, and that now ramps across every level rather than stepping twice.
    activePositions = level <= 3 ? &easyPositions
                                 : (level <= 6 ? &mediumPositions : &hardPositions);

    tolerancePan = toleranceForLevel (level);
}

juce::String PanGame::formatNormalisedValue (float normalised) const
{
    const auto pan = normalisedToPan (normalised);
    const auto percent = juce::roundToInt (std::abs (pan) * 100.0f);

    // Minus to the left, plus to the right, zero in the middle - the
    // author's call: "L50/R50" read as letters to decode, a signed number
    // reads as a position on a ruler, which is what this is.
    if (percent < 3)
        return "0";

    return (pan < 0.0f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : juce::String ("+")) + juce::String (percent);
}

std::vector<Game::GridMark> PanGame::getGridMarks() const
{
    std::vector<GridMark> marks;

    // Every 10% of the field, with the labelled quarters emphasised - the
    // same two-density ruler the frequency scale uses.
    for (int percent = -100; percent <= 100; percent += 10)
    {
        const auto pan = (float) percent / 100.0f;
        const auto emphasised = (percent % 50 == 0);
        marks.push_back ({ panToNormalised (pan), formatNormalisedValue (panToNormalised (pan)), emphasised });
    }

    return marks;
}

void PanGame::submitNormalisedAnswer (float normalised)
{
    if (answered)
        return;

    chosenNormalised = juce::jlimit (0.0f, 1.0f, normalised);
    chosenPositionIndex = -1;

    lastAnswerCorrect = std::abs (normalisedToPan (chosenNormalised) - targetPan) <= tolerancePan;

    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

void PanGame::newRound()
{
    // Anywhere across the field, not one of five points.
    // Uniform across the field, asked more often where this player misses
    // (Game::keepDraw).
    for (int attempt = 0; attempt < 16; ++attempt)
    {
        targetPan = random.nextFloat() * 2.0f - 1.0f;
        if (keepDraw (random, bucketForPan (targetPan)))
            break;
    }

    // Nearest named position, for the legacy discrete path only.
    correctPositionIndex = 0;
    auto smallestDistance = std::numeric_limits<float>::max();
    for (int i = 0; i < numPositions; ++i)
    {
        const auto distance = std::abs ((*activePositions)[(size_t) i] - targetPan);
        if (distance < smallestDistance)
        {
            smallestDistance = distance;
            correctPositionIndex = i;
        }
    }

    chosenNormalised = -1.0f;
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
           + "It was panned " + formatNormalisedValue (panToNormalised (targetPan)) + ".";
}

float PanGame::toleranceForLevel (int level) noexcept
{
    // Level 1 was +-35% of the field: with the hint at a couple of bands
    // either side, the shaded "somewhere here" covered the whole ruler.
    return rampTolerance (level, 0.25f, 0.07f);
}

Game::LevelMeaning PanGame::describeLevel (int level) const
{
    LevelMeaning meaning;
    meaning.unit = LevelMeaning::Unit::percent;
    meaning.tolerance = toleranceForLevel (level) * 100.0f;
    return meaning;
}

#pragma once

#include "Game.h"
#include "../TestSignalGenerator.h"
#include <array>

// "Guess the pan position" exercise: continuous pink noise panned to one
// of five fixed positions, using an equal-power pan law (constant total
// energy regardless of position, so no separate loudness compensation is
// needed - unlike CompressionGame/DistortionGame's tuned-by-ear makeup
// gain, this game gets loudness equalization for free from the pan law
// itself).
class PanGame : public Game
{
public:
    static constexpr int numPositions = 5;

    juce::String getName() const override { return "Guess the Pan Position"; }
    juce::String getInstructions() const override
    {
        return "Listen, then guess where the sound is panned. Equal-power "
               "panning keeps loudness constant across positions, so trust "
               "your sense of direction rather than loudness.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;
    void setDifficulty (int level) override;
    void setReferenceAudioLibrary (const ReferenceAudioLibrary* library) override { noise.setLibrary (library); }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    int getNumChoices() const override { return numPositions; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctPositionIndex; }
    int getChosenChoiceIndex() const override { return chosenPositionIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

private:
    static const std::array<const char*, numPositions> positionLabels;

    // Same labels throughout (Hard Left/Left/Center/Right/Hard Right);
    // only how far apart the underlying pan values sit changes - easy
    // (levels 1-3) spans the full -1..+1 range, medium/hard (4-6/7-10)
    // converge the non-center positions toward the middle, same
    // "same labels, converging values" pattern as CompressionGame's
    // three preset tables.
    static const std::array<float, numPositions> easyPositions;
    static const std::array<float, numPositions> mediumPositions;
    static const std::array<float, numPositions> hardPositions;
    const std::array<float, numPositions>* activePositions = &easyPositions;

    TestSignalGenerator noise;

    juce::Random random;

    int correctPositionIndex = 0;
    int chosenPositionIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;
};

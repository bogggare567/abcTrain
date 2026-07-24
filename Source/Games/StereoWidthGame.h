#pragma once

#include "Game.h"
#include "../PinkNoiseGenerator.h"
#include <array>

// "Guess the stereo width" exercise: two independent PinkNoiseGenerator
// instances (naturally decorrelated - each owns its own juce::Random
// stream) combined via mid/side processing at one of four fixed width
// multipliers. A single mono noise source duplicated to both channels
// would have zero side signal and nothing for width to affect, which is
// why this game needs two independent generators where every other game
// here uses one.
//
// Same qualitative labels throughout (Narrow/Normal/Wide/Extra Wide, no
// numbers in the label itself); setDifficulty converges the underlying
// width multipliers toward 1.0 (unchanged) at harder tiers - the medium
// tier's values are exactly the 30/70/100/130% the original spec named,
// same "same labels, converging values" shape as CompressionGame/PanGame.
class StereoWidthGame : public Game
{
public:
    static constexpr int numWidths = 4;

    juce::String getName() const override { return "Guess the Stereo Width"; }
    juce::String getInstructions() const override
    {
        return "Listen, then guess how wide the stereo image is.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;
    void setDifficulty (int level) override;

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    int getNumChoices() const override { return numWidths; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctWidthIndex; }
    int getChosenChoiceIndex() const override { return chosenWidthIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

private:
    static const std::array<const char*, numWidths> widthLabels;
    static const std::array<float, numWidths> easyWidths;
    static const std::array<float, numWidths> mediumWidths;
    static const std::array<float, numWidths> hardWidths;
    const std::array<float, numWidths>* activeWidths = &easyWidths;

    PinkNoiseGenerator noiseL, noiseR;

    juce::Random random;

    int correctWidthIndex = 0;
    int chosenWidthIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;
};

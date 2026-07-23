#pragma once

#include "Game.h"
#include "../PinkNoiseGenerator.h"
#include <array>

// "Guess the band" exercise: plays pink noise through a peak filter boosting
// or cutting one random octave band, and scores the player's guess.
class EQGame : public Game
{
public:
    static constexpr int numBands = 8;
    static const std::array<float, numBands> bandFrequenciesHz;

    juce::String getName() const override { return "Guess the Band"; }
    juce::String getInstructions() const override
    {
        return "Listen, then click the band you think was boosted or cut.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    int getNumChoices() const override { return numBands; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctBandIndex; }
    int getChosenChoiceIndex() const override { return chosenBandIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

private:
    void updateFilter();

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> peakFilter;
    double sampleRate = 44100.0;

    juce::Random random;
    PinkNoiseGenerator noise;

    int correctBandIndex = 0;
    int chosenBandIndex = -1;
    bool isBoost = true;
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;

    static constexpr float gainDb = 9.0f;
    static constexpr float filterQ = 2.0f;
};

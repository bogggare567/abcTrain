#pragma once

#include "Game.h"
#include "../PinkNoiseGenerator.h"
#include <array>

// "Name the range" exercise: plays pink noise through a peak filter
// boosting or cutting a random frequency inside one of 7 standard named
// frequency ranges (sub-bass, bass, low-mids, mids, high-mids, presence,
// air - the same industry-standard range names used in
// docs/knowledge_base.md and LearnerEQ's FrequencyGuide), and scores the
// player's guess of which named range it fell in. Unlike EQGame (which
// asks "which of 8 fixed octave bands"), the boosted frequency here moves
// around within the chosen range each round, so the player learns the
// range's boundaries rather than memorizing 8 fixed points.
class FrequencyRangeGame : public Game
{
public:
    static constexpr int numRanges = 7;

    struct Range
    {
        const char* label;
        float lowHz;
        float highHz;
    };

    static const std::array<Range, numRanges> ranges;

    juce::String getName() const override { return "Name the Range"; }
    juce::String getInstructions() const override
    {
        return "Listen, then click the frequency range you think was boosted or cut.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;
    void setDifficulty (int level) override;

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    int getNumChoices() const override { return numRanges; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctRangeIndex; }
    int getChosenChoiceIndex() const override { return chosenRangeIndex; }
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

    int correctRangeIndex = 0;
    float correctFreqHz = 100.0f;
    int chosenRangeIndex = -1;
    bool isBoost = true;
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;

    // Easy (levels 1-3): 9 dB. Medium (4-6): 6 dB. Hard (7-10): 3 dB -
    // same "fixed labels, scaled gain" shape as EQGame, its closest
    // precedent.
    float gainDb = 9.0f;
    static constexpr float filterQ = 2.0f;
};

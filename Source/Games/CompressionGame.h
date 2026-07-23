#pragma once

#include "Game.h"
#include "../PinkNoiseGenerator.h"
#include <array>

// "Guess the compression" exercise: plays a repeating percussive noise
// burst through juce::dsp::Compressor at one of three fixed weak/medium/
// strong presets. Output loudness is makeup-gain-matched across presets
// so the player has to judge compression character (pumping, reduced
// dynamics) rather than just picking whichever answer sounds loudest.
class CompressionGame : public Game
{
public:
    static constexpr int numLevels = 3;

    juce::String getName() const override { return "Guess the Compression"; }
    juce::String getInstructions() const override
    {
        return "Listen to the drum hits, then guess how strong the compression is.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    int getNumChoices() const override { return numLevels; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctLevelIndex; }
    int getChosenChoiceIndex() const override { return chosenLevelIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

private:
    struct Preset
    {
        const char* label;
        float thresholdDb;
        float ratio;
        // Fixed compensation tuned by ear so the three presets sit at
        // roughly equal perceived loudness; not computed from measured
        // gain reduction.
        float makeupGainDb;
    };

    static const std::array<Preset, numLevels> presets;

    void updateCompressor();

    juce::dsp::Compressor<float> compressor;
    PinkNoiseGenerator noise;
    double sampleRate = 44100.0;

    int samplesSinceBurstStart = 0;
    int attackSamples = 1;
    int decayTauSamples = 1;
    int burstPeriodSamples = 1;

    juce::Random random;

    int correctLevelIndex = 0;
    int chosenLevelIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;
};

#pragma once

#include "Game.h"
#include "../TestSignalGenerator.h"
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
        return "Listen, then click the band you think was boosted or cut. A "
               "boost in the middle of the spectrum is often the easiest "
               "kind of change to hear - sometimes from just 2-3 dB.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;
    void setDifficulty (int level) override;
    void setReferenceAudioLibrary (const ReferenceAudioLibrary* library) override { noise.setLibrary (library); }

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
    TestSignalGenerator noise;

    int correctBandIndex = 0;
    int chosenBandIndex = -1;
    bool isBoost = true;
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;

    // Easy (levels 1-3): 9 dB. Medium (4-6): 6 dB. Hard (7-10): 3 dB.
    // Smaller boost/cut is harder to hear. Set via setDifficulty().
    float gainDb = 9.0f;
    static constexpr float filterQ = 2.0f;
};

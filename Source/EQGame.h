#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>
#include <array>

// "Guess the band" exercise: plays pink noise through a peak filter boosting
// or cutting one random octave band, and scores the player's guess.
class EQGame : public juce::ChangeBroadcaster
{
public:
    static constexpr int numBands = 8;
    static const std::array<float, numBands> bandFrequenciesHz;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);

    void newRound();
    void submitAnswer (int bandIndex);

    bool hasAnswered() const noexcept { return answered; }
    int getCorrectBandIndex() const noexcept { return correctBandIndex; }
    int getChosenBandIndex() const noexcept { return chosenBandIndex; }
    bool wasLastAnswerCorrect() const noexcept { return lastAnswerCorrect; }
    bool wasBoost() const noexcept { return isBoost; }

    int getScore() const noexcept { return correctCount; }
    int getRoundsPlayed() const noexcept { return totalCount; }

private:
    void updateFilter();
    float nextPinkNoiseSample();

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> peakFilter;
    double sampleRate = 44100.0;

    juce::Random random;

    // Paul Kellet's "economy" pink noise filter state (b0..b6).
    float pinkState[7] = {};

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

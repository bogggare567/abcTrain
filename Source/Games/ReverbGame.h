#pragma once

#include "Game.h"
#include "../PinkNoiseGenerator.h"
#include <array>

// "Guess the reverb type" exercise: a repeating percussive noise burst
// through one of four reverb characters. Room/Hall/Plate are approximated
// with different juce::dsp::Reverb (Freeverb-derived algorithm) parameter
// presets - not physically modeled per type, just tuned by ear to sound
// distinct, same approach CompressionGame takes with its presets. Spring
// is different enough (a metallic comb/allpass "boing" that Freeverb-style
// algorithms don't produce) that it's built separately, as a cascade of
// resonant allpass filters instead of the Reverb DSP object.
class ReverbGame : public Game
{
public:
    static constexpr int numTypes = 4;

    juce::String getName() const override { return "Guess the Reverb"; }
    juce::String getInstructions() const override
    {
        return "Listen to the repeating hit, then guess the reverb type.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;
    void setDifficulty (int level) override;

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    // Easy (levels 1-3): Room/Hall only. Medium (4-6): + Plate. Hard
    // (7-10): all four including Spring. Fewer choices = easier, and the
    // array order (Room, Hall, Plate, Spring) is deliberately most- to
    // least-distinguishable, so the "easy" subset is genuinely easy.
    int getNumChoices() const override { return activeNumTypes; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctTypeIndex; }
    int getChosenChoiceIndex() const override { return chosenTypeIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

private:
    static constexpr int springTypeIndex = 3;
    static constexpr float springQ = 4.0f;
    static const std::array<float, 4> springFrequenciesHz;
    static const std::array<const char*, numTypes> typeLabels;

    void updateReverbForType();

    PinkNoiseGenerator noise;
    juce::dsp::Reverb reverb;
    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                               juce::dsp::IIR::Coefficients<float>>, 4> springAllpass;
    double sampleRate = 44100.0;
    // Defaults to the easy tier (matches EQGame/CompressionGame both
    // defaulting to their easy values) in case something ever constructs
    // a game and calls newRound() before setDifficulty() - the real app
    // always calls setDifficulty() during ProgressManager construction,
    // before the host's first prepareToPlay(), so this default is a
    // defensive fallback, not something normally observed.
    int activeNumTypes = 2;

    int samplesSinceBurstStart = 0;
    int attackSamples = 1;
    int decayTauSamples = 1;
    int burstPeriodSamples = 1;

    juce::Random random;

    int correctTypeIndex = 0;
    int chosenTypeIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;
};

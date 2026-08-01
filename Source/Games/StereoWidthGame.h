#pragma once

#include "Game.h"
#include "../../shared/PinkNoiseGenerator.h"
#include <array>
#include <atomic>
#include <vector>
#include "../../shared/PresetFamily.h"

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
        return "Listen, then guess how wide the stereo image is. Width "
               "comes from decorrelation between channels, not just "
               "panning - always worth checking a wide mix still holds "
               "together in mono.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;
    void setDifficulty (int level) override;

    // A/B - see Game::supportsBeforeAfter. "Before" collapses the side
    // signal to nothing, so the comparison is against genuine mono: width
    // is the one quantity whose natural reference point is its absence,
    // and hearing the same noise snap between mono and the round's width
    // is what makes "Narrow" mean something.
    bool supportsBeforeAfter() const override { return true; }
    void setPlayProcessed (bool shouldPlayProcessed) override { playProcessed.store (shouldPlayProcessed); }
    bool isPlayingProcessed() const override { return playProcessed.load(); }
    juce::String getBeforeLabel() const override { return "Mono"; }
    juce::String getAfterLabel() const override { return "In Stereo"; }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    // **Always two** - see ReverbGame for why. More buttons is more
    // reading and more luck, not finer hearing; two alternatives makes the
    // question "which of these", and difficulty becomes how close together
    // they are.
    int getNumChoices() const override { return 2; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctWidthIndex; }
    int getChosenChoiceIndex() const override { return chosenWidthIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

public:
    // Width is one number, so a "family" here can't mean four different
    // settings of it - that would just be the neighbouring category. What
    // it means instead is four ways of *arriving* at the same width, and
    // the one that matters in real work is how much of the low end is
    // left in mono. Two mixes at the same nominal width sound
    // meaningfully different when one of them keeps everything under
    // 150 Hz centred, and learning to hear past that is the point: the
    // width you notice is mostly the width above the bass.
    struct Variant
    {
        float monoBelowHz = 0.0f;   // 0 = the side signal is widened whole
        float archetypal = 1.0f;
    };

    static const std::vector<Variant>& family();

private:
    static const std::array<const char*, numWidths> widthLabels;
    // Computed in setDifficulty from one ramped spread value - see there
    // for why this replaced three fixed tables.
    std::array<float, numWidths> widths { { 0.1f, 0.6f, 1.0f, 1.6f } };

    PinkNoiseGenerator noiseL, noiseR;

    juce::Random random;

    // Read on the audio thread every block, written from the UI.
    std::atomic<bool> playProcessed { true };

    float roundWidthJitter = 0.0f;

    // Settled in newRound on the message thread; the audio thread reads
    // the coefficient and the running state only.
    Variant roundVariant;
    float monoSplitCoeff = 0.0f;    // 0 = no split, side widened whole
    float sideLowStateL = 0.0f;
    double sampleRate = 44100.0;

    // The two categories on offer this round. correctWidthIndex is 0 or 1 into
    // this, not an index into the full list.
    std::array<int, 2> pairIndices { { 0, 1 } };
    int difficultyLevel = 1;

    // Width is already one axis, so the positions are the widths
    // themselves, normalised - no interpretation needed.
    static const std::vector<float>& axisPositions();

    int correctWidthIndex = 0;
    int chosenWidthIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;
};

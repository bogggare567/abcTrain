#pragma once

#include "Game.h"
#include <atomic>
#include "../../shared/TestSignalGenerator.h"
#include <array>

// "Guess the gain change" exercise: continuous pink noise at one of five
// fixed gain deltas relative to a reference level (-2*step, -step, 0,
// +step, +2*step). This is the one game of the five where the choice
// *labels* themselves must change with difficulty rather than staying
// fixed - the numbers are the whole point here, unlike Pan/StereoWidth's
// qualitative names, so there's no way to keep the same label text while
// making the underlying gap smaller. setDifficulty instead shrinks the
// step size: easy (1-3) is +/-6 dB steps, medium (4-6) is +/-3 dB (this
// is exactly the -6/-3/0/+3/+6 dB set the original spec named), hard
// (7-10) is +/-2 dB - smaller gain differences are objectively harder to
// hear, a well-established perceptual fact, not a guess.
class DBGame : public Game
{
public:
    static constexpr int numChoices = 5;

    juce::String getName() const override { return "Guess the Gain Change"; }
    juce::String getInstructions() const override
    {
        return "Listen, then set the scale to how much you think the level "
               "changed. Differences below roughly 1 dB get hard to tell "
               "apart at all, so the tolerance never asks for more than "
               "that.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;
    void setDifficulty (int level) override;
    void setReferenceAudioLibrary (const ReferenceAudioLibrary* library) override { noise.setLibrary (library); }

    // A/B - comparing the treated signal against the untreated one is
    // how a change is actually heard; see Game::supportsBeforeAfter.
    bool supportsBeforeAfter() const override { return true; }
    void setPlayProcessed (bool shouldPlayProcessed) override { playProcessed.store (shouldPlayProcessed); }
    bool isPlayingProcessed() const override { return playProcessed.load(); }
    juce::String getBeforeLabel() const override { return "Before Gain"; }
    juce::String getAfterLabel() const override { return "After Gain"; }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    // Continuous: the level change is any value in the range, and the
    // accept band narrows with difficulty. Five fixed steps meant the
    // answer was always one of five round numbers, so a player learned
    // the *set* rather than the sound of a decibel.
    bool usesContinuousScale() const override { return true; }
    float getToleranceNormalised() const override { return toleranceDb / axisSpanDb; }
    float getCorrectNormalised() const override { return dbToNormalised (targetDb); }
    float getChosenNormalised() const override { return chosenNormalised; }
    juce::String formatNormalisedValue (float normalised) const override;
    void submitNormalisedAnswer (float normalised) override;
    std::vector<GridMark> getGridMarks() const override;

    // Linear in dB, which is already the perceptual unit.
    static constexpr float axisMinDb = -9.0f;
    static constexpr float axisMaxDb = 9.0f;
    static constexpr float axisSpanDb = axisMaxDb - axisMinDb;

    static float normalisedToDb (float normalised) noexcept
    {
        return axisMinDb + normalised * axisSpanDb;
    }

    static float dbToNormalised (float db) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, (db - axisMinDb) / axisSpanDb);
    }

    int getNumChoices() const override { return numChoices; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctChoiceIndex; }
    int getChosenChoiceIndex() const override { return chosenChoiceIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    // See Game::getHintView.
    HintView getHintView() const override { return HintView::envelope; }

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

private:
    static juce::String formatDb (int db);

    float getDeltaDb (int choiceIndex) const { return (float) ((choiceIndex - 2) * stepDb); }

    TestSignalGenerator noise;

    // Easy: 6. Medium: 3 (the -6/-3/0/+3/+6 dB set from the original
    // spec). Hard: 2.
    int stepDb = 6;

    juce::Random random;

    // The real answer, anywhere in the range.
    float targetDb = 0.0f;
    float chosenNormalised = -1.0f;

    // Half-width of the accept band, in dB. 1 dB is about where a level
    // difference stops being reliably audible at all, so the hard tier
    // sits just above that rather than below it.
    float toleranceDb = 2.5f;

    int correctChoiceIndex = 0;
    int chosenChoiceIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    // Read on the audio thread every block, written from the UI.
    std::atomic<bool> playProcessed { true };

    int correctCount = 0;
    int totalCount = 0;
};

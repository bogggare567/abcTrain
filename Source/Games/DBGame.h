#pragma once

#include "Game.h"
#include "../TestSignalGenerator.h"
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
        return "Listen, then guess how much the level changed.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;
    void setDifficulty (int level) override;
    void setReferenceAudioLibrary (const ReferenceAudioLibrary* library) override { noise.setLibrary (library); }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    int getNumChoices() const override { return numChoices; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctChoiceIndex; }
    int getChosenChoiceIndex() const override { return chosenChoiceIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

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

    int correctChoiceIndex = 0;
    int chosenChoiceIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;
};

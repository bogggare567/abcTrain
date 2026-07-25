#pragma once

#include "Game.h"
#include "../TestSignalGenerator.h"
#include <array>

// "Guess the delay time" exercise: a percussive noise burst through a
// single feedback=0, 50/50 dry/wet echo (juce::dsp::DelayLine) at one of
// four fixed delay times. Those two mix parameters are fixed by design,
// not tiered - the four delay-time choices always stay literally
// 50/150/300/500 ms. setDifficulty instead scales how isolated each
// burst is: a longer gap between bursts at easy levels lets the single
// echo ring out in silence; a shorter gap at hard levels means the next
// burst's attack can mask exactly when the echo landed, same "burst
// period" lever CompressionGame/ReverbGame already use for their own
// difficulty scaling.
class DelayGame : public Game
{
public:
    static constexpr int numDelayTimes = 4;

    juce::String getName() const override { return "Guess the Delay Time"; }
    juce::String getInstructions() const override
    {
        return "Listen to the echo, then guess the delay time. Very short "
               "delay times blur into the source instead of sounding like a "
               "distinct echo - the same effect behind widening and chorus "
               "tricks.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;
    void setDifficulty (int level) override;
    void setReferenceAudioLibrary (const ReferenceAudioLibrary* library) override { noise.setLibrary (library); }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    int getNumChoices() const override { return numDelayTimes; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctDelayIndex; }
    int getChosenChoiceIndex() const override { return chosenDelayIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

private:
    static constexpr float dryWetFraction = 0.5f;
    static const std::array<float, numDelayTimes> delayTimesMs;

    void updateDelayTime();
    // Converts burstPeriodSeconds -> burstPeriodSamples using whatever
    // sampleRate is currently known. Called from both prepare() (first
    // real sample rate) and setDifficulty() (in case difficulty changes
    // again mid-session, after sampleRate already holds the real value).
    void updateBurstPeriod();

    TestSignalGenerator noise;
    juce::dsp::DelayLine<float> delayLine { 96000 };
    double sampleRate = 44100.0;

    int samplesSinceBurstStart = 0;
    int attackSamples = 1;
    int decayTauSamples = 1;
    // Defaults to the easy tier, same defensive-default reasoning as
    // ReverbGame's activeNumTypes.
    float burstPeriodSeconds = 1.4f;
    int burstPeriodSamples = 1;

    juce::Random random;

    int correctDelayIndex = 0;
    int chosenDelayIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;
};

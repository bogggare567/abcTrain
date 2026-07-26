#pragma once

#include "Game.h"
#include <atomic>
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
        return "Listen to the echo, then set the scale to how long the "
               "delay is. Very short delay times blur into the source "
               "instead of sounding like a distinct echo - the same effect "
               "behind widening and chorus tricks.";
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
    juce::String getBeforeLabel() const override { return "Dry"; }
    juce::String getAfterLabel() const override { return "With Echo"; }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    // Continuous: any delay time in the range, with an accept band that
    // is a constant *ratio* rather than a constant number of
    // milliseconds - 20 ms out at a 40 ms delay is a completely different
    // error from 20 ms out at 500 ms, and only the ratio matches how the
    // ear hears it. That is also why the axis is logarithmic.
    bool usesContinuousScale() const override { return true; }
    float getToleranceNormalised() const override;
    float getCorrectNormalised() const override { return msToNormalised (targetMs); }
    float getChosenNormalised() const override { return chosenNormalised; }
    juce::String formatNormalisedValue (float normalised) const override;
    void submitNormalisedAnswer (float normalised) override;
    std::vector<GridMark> getGridMarks() const override;

    static constexpr float axisMinMs = 20.0f;
    static constexpr float axisMaxMs = 640.0f;

    static float normalisedToMs (float normalised) noexcept;
    static float msToNormalised (float ms) noexcept;

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

    // The real answer, anywhere in the range.
    float targetMs = 150.0f;
    float chosenNormalised = -1.0f;

    // Half-width of the accept band, as a ratio (0.35 means "within
    // +/-35% of the true time").
    float toleranceRatio = 0.35f;

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

    // Read on the audio thread every block, written from the UI.
    std::atomic<bool> playProcessed { true };

    int correctCount = 0;
    int totalCount = 0;
};

#pragma once

#include "Game.h"
#include <atomic>
#include "../../shared/TestSignalGenerator.h"
#include <array>

// "Find the frequency" exercise: plays pink noise through a peak filter
// boosting or cutting one frequency, and scores how close the player's
// guess lands.
//
// The target is drawn log-uniformly from the whole 100 Hz - 12.8 kHz
// range, not snapped to one of eight octave centres. Fixed band centres
// were memorisable as *positions* without ever learning what the
// frequencies sound like, and they made the exercise a multiple-choice
// quiz rather than an ear test. Difficulty now narrows the accept band
// (1 octave -> 0.6 -> 0.35) instead of only shrinking the boost.
//
// The eight octave frequencies survive as the labelled grid marks on the
// scale, which is also what keeps the discrete getNumChoices()/
// submitAnswer(int) path working for anything that still uses it.
class EQGame : public Game
{
public:
    static constexpr int numBands = 8;
    static const std::array<float, numBands> bandFrequenciesHz;

    juce::String getName() const override { return "Guess the Band"; }
    juce::String getInstructions() const override
    {
        return "Listen, then drag along the scale to where you think the "
               "boost or cut is. You don't have to be exact - land inside "
               "the tolerance band and it counts. A boost in the middle of "
               "the spectrum is often the easiest kind of change to hear.";
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
    juce::String getBeforeLabel() const override { return "EQ Off"; }
    juce::String getAfterLabel() const override { return "EQ On"; }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    bool usesContinuousScale() const override { return true; }
    float getToleranceNormalised() const override;
    float getCorrectNormalised() const override { return frequencyToNormalised (targetHz); }
    float getChosenNormalised() const override { return chosenNormalised; }
    juce::String formatNormalisedValue (float normalised) const override;
    void submitNormalisedAnswer (float normalised) override;

    // The scale runs log-uniformly between the outermost grid marks, so
    // the labelled 100 Hz and 12.8 kHz sit exactly at its two ends.
    static float normalisedToFrequency (float normalised) noexcept;
    static float frequencyToNormalised (float hz) noexcept;

    // Octave centres (100 Hz .. 12.8 kHz) as the emphasised series, plus
    // the half-octave boundaries between them as a quieter second series -
    // the same two-row ruler the reference trainers use, which gives the
    // eye something to interpolate against between the round numbers.
    std::vector<GridMark> getGridMarks() const override;

    // The axis extends half an octave past the outermost centres, so the
    // 100 Hz and 12.8 kHz marks sit inside the scale rather than on its
    // edges where half their label would clip.
    static constexpr float axisLowHz = 70.71f;    // 100 / sqrt(2)
    static constexpr float axisHighHz = 18101.9f; // 12800 * sqrt(2)
    static constexpr float axisOctaves = 8.0f;

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

    // The real answer, anywhere in the range - not one of numBands.
    float targetHz = 1000.0f;
    float chosenNormalised = -1.0f;

    // Nearest grid mark to targetHz, so the legacy discrete path
    // (submitAnswer(int)/getCorrectChoiceIndex()) still has an index to
    // talk about and keeps its original exact-match semantics.
    int correctBandIndex = 0;
    int chosenBandIndex = -1;
    bool isBoost = true;
    bool answered = false;
    bool lastAnswerCorrect = false;

    // Read on the audio thread every block, written from the UI.
    std::atomic<bool> playProcessed { true };

    int correctCount = 0;
    int totalCount = 0;

    // Easy (levels 1-3): 9 dB. Medium (4-6): 6 dB. Hard (7-10): 3 dB.
    // Smaller boost/cut is harder to hear. Set via setDifficulty().
    float gainDb = 9.0f;

    // Half-width of the accept band, in octaves, by the same tier split.
    float toleranceOctaves = 1.0f;
    static constexpr float filterQ = 2.0f;
};

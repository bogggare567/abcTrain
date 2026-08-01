#pragma once

#include "Game.h"
#include "../../shared/TestSignalGenerator.h"
#include <array>
#include <vector>
#include "../../shared/PresetFamily.h"

// "Name the range" exercise: plays pink noise through a peak filter
// boosting or cutting a random frequency inside one of 7 standard named
// frequency ranges (sub-bass, bass, low-mids, mids, high-mids, presence,
// air - the same industry-standard range names used in
// docs/knowledge_base.md and LearnerEQ's FrequencyGuide), and scores the
// player's guess of which named range it fell in. Unlike EQGame (which
// asks "which of 8 fixed octave bands"), the boosted frequency here moves
// around within the chosen range each round, so the player learns the
// range's boundaries rather than memorizing 8 fixed points.
class FrequencyRangeGame : public Game
{
public:
    static constexpr int numRanges = 7;

    struct Range
    {
        const char* label;
        float lowHz;
        float highHz;
    };

    static const std::array<Range, numRanges> ranges;

    juce::String getName() const override { return "Name the Range"; }
    juce::String getInstructions() const override
    {
        return "Listen, then click the frequency range you think was "
               "boosted or cut. Treat the named ranges as a starting map "
               "for where a problem tends to live, not a fixed rule.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;
    void setDifficulty (int level) override;
    void setReferenceAudioLibrary (const ReferenceAudioLibrary* library) override { noise.setLibrary (library); }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    // **Always two** - see ReverbGame for why. More buttons is more
    // reading and more luck, not finer hearing; two alternatives makes the
    // question "which of these", and difficulty becomes how close together
    // they are.
    int getNumChoices() const override { return 2; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctRangeIndex; }
    int getChosenChoiceIndex() const override { return chosenRangeIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

    // Naming an absolute range is the one exercise where a reference is
    // not a luxury - see the note in process().
    bool supportsBeforeAfter() const override { return true; }
    void setPlayProcessed (bool shouldPlayProcessed) override { playProcessed.store (shouldPlayProcessed); }
    bool isPlayingProcessed() const override { return playProcessed.load(); }
    // "Filtered", not "Boosted": this game cuts as often as it boosts, so
    // the old label was a lie half the time - and worse, a lie that told
    // the player which way the change went before they had listened.
    juce::String getBeforeLabel() const override { return "Flat"; }
    juce::String getAfterLabel() const override { return "Filtered"; }

private:
    std::atomic<bool> playProcessed { true };

    void updateFilter();

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> peakFilter;
    double sampleRate = 44100.0;

    juce::Random random;
    TestSignalGenerator noise;

    // The two categories on offer this round. correctRangeIndex is 0 or 1 into
    // this, not an index into the full list.
    std::array<int, 2> pairIndices { { 0, 1 } };
    int difficultyLevel = 1;

    // Spectral order. Neighbouring ranges share a boundary and really are
    // confusable; sub-bass against air is not a listening question.
    static const std::vector<float>& axisPositions();

    int correctRangeIndex = 0;
    float correctFreqHz = 100.0f;

public:
    // Test seams: which frequency this round actually boosted, and how
    // wide a bump it was. Both are claims worth checking - a frequency
    // that wanders outside its own named range would be scoring a correct
    // answer as wrong.
    float getCorrectFrequencyHzForTest() const { return correctFreqHz; }
    float getFilterQForTest() const { return filterQ; }
    int getCorrectRangeForTest() const { return pairIndices[(size_t) correctRangeIndex]; }

private:
    int chosenRangeIndex = -1;
    bool isBoost = true;

public:
    // See Game::getAnswerDirection/getAnswerDetail. The detail is the
    // exact frequency - numbers and units only, so it needs no
    // translation; the range's *name* is the winning choice label, which
    // the editor translates like any other.
    int getAnswerDirection() const override { return isBoost ? 1 : -1; }
    juce::String getAnswerDetail() const override
    {
        return correctFreqHz >= 1000.0f
                 ? juce::String (correctFreqHz / 1000.0f, 1) + " kHz"
                 : juce::String (juce::roundToInt (correctFreqHz)) + " Hz";
    }

private:
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;

    // Easy (levels 1-3): 9 dB. Medium (4-6): 6 dB. Hard (7-10): 3 dB -
    // same "fixed labels, scaled gain" shape as EQGame, its closest
    // precedent.
    float gainDb = 9.0f;
    // Redrawn each round rather than fixed - see newRound. A broad bump
    // lifts a whole named range; a narrow one is one tone inside it.
    float filterQ = 2.0f;
};

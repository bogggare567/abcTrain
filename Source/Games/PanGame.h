#pragma once

#include "Game.h"
#include <atomic>
#include "../../shared/TestSignalGenerator.h"
#include <array>

// "Guess the pan position" exercise: continuous pink noise panned to one
// of five fixed positions, using an equal-power pan law (constant total
// energy regardless of position, so no separate loudness compensation is
// needed - unlike CompressionGame/DistortionGame's tuned-by-ear makeup
// gain, this game gets loudness equalization for free from the pan law
// itself).
class PanGame : public Game
{
public:
    static constexpr int numPositions = 5;

    juce::String getName() const override { return "Guess the Pan Position"; }
    juce::String getInstructions() const override
    {
        return "Listen, then point to where the sound sits in the stereo "
               "field. Equal-power panning keeps loudness constant across "
               "positions, so trust your sense of direction rather than "
               "loudness.";
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
    juce::String getBeforeLabel() const override { return "Centred"; }
    juce::String getAfterLabel() const override { return "Panned"; }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    // Continuous: the target sits anywhere across the stereo field, and
    // the accept band narrows with difficulty. Five named positions never
    // taught where a sound actually *is* - only which of five words to
    // pick - and "Left" is not a thing you can dial on a console.
    bool usesContinuousScale() const override { return true; }
    float getToleranceNormalised() const override { return tolerancePan * 0.5f; }
    float getCorrectNormalised() const override { return panToNormalised (targetPan); }
    float getChosenNormalised() const override { return chosenNormalised; }
    juce::String formatNormalisedValue (float normalised) const override;
    void submitNormalisedAnswer (float normalised) override;
    std::vector<GridMark> getGridMarks() const override;

    // Pan runs -1 (hard left) .. +1 (hard right); the axis is linear,
    // because equal-power panning already makes perceived position track
    // the control linearly.
    static float normalisedToPan (float normalised) noexcept { return normalised * 2.0f - 1.0f; }
    static float panToNormalised (float pan) noexcept { return (pan + 1.0f) * 0.5f; }

    int getNumChoices() const override { return numPositions; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctPositionIndex; }
    int getChosenChoiceIndex() const override { return chosenPositionIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

private:
    static const std::array<const char*, numPositions> positionLabels;

    // Same labels throughout (Hard Left/Left/Center/Right/Hard Right);
    // only how far apart the underlying pan values sit changes - easy
    // (levels 1-3) spans the full -1..+1 range, medium/hard (4-6/7-10)
    // converge the non-center positions toward the middle, same
    // "same labels, converging values" pattern as CompressionGame's
    // three preset tables.
    static const std::array<float, numPositions> easyPositions;
    static const std::array<float, numPositions> mediumPositions;
    static const std::array<float, numPositions> hardPositions;
    const std::array<float, numPositions>* activePositions = &easyPositions;

    TestSignalGenerator noise;

    juce::Random random;

    // The real answer, anywhere in -1..+1.
    float targetPan = 0.0f;
    float chosenNormalised = -1.0f;

    // Half-width of the accept band, in pan units (so 0.25 means a
    // quarter of the way to a speaker either side).
    float tolerancePan = 0.25f;

    // Nearest named position to targetPan, keeping the legacy discrete
    // path's exact-match semantics intact.
    int correctPositionIndex = 0;
    int chosenPositionIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    // Read on the audio thread every block, written from the UI.
    std::atomic<bool> playProcessed { true };

    int correctCount = 0;
    int totalCount = 0;
};

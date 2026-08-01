#pragma once

#include "Game.h"
#include <atomic>
#include "../../shared/TestSignalGenerator.h"
#include <array>
#include <vector>
#include "../../shared/PresetFamily.h"

// "Guess the compression" exercise: plays a repeating percussive noise
// burst through juce::dsp::Compressor at one of three fixed weak/medium/
// strong presets. Output loudness is makeup-gain-matched across presets
// so the player has to judge compression character (pumping, reduced
// dynamics) rather than just picking whichever answer sounds loudest.
class CompressionGame : public Game
{
public:
    static constexpr int numLevels = 3;

    juce::String getName() const override { return "Guess the Compression"; }
    juce::String getInstructions() const override
    {
        return "Listen to the drum hits, then guess how strong the "
               "compression is. Judge it by how much the loud hits get "
               "pulled down relative to the quiet ones, not just by overall "
               "loudness.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;
    void setDifficulty (int level) override;
    void setReferenceAudioLibrary (const ReferenceAudioLibrary* library) override { noise.setLibrary (library); }
    void setNoiseColour (NoiseColour colour) override { noise.setNoiseColour (colour); }

    // A/B - comparing the treated signal against the untreated one is
    // how a change is actually heard; see Game::supportsBeforeAfter.
    bool supportsBeforeAfter() const override { return true; }
    void setPlayProcessed (bool shouldPlayProcessed) override { playProcessed.store (shouldPlayProcessed); }
    bool isPlayingProcessed() const override { return playProcessed.load(); }
    juce::String getBeforeLabel() const override { return "Comp Off"; }
    juce::String getAfterLabel() const override { return "Comp On"; }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    // Always two - see ReverbGame for the reasoning. Here the pair is
    // drawn from Weak/Medium/Strong, and a level decides whether you get
    // the two ends (easy) or two neighbours (hard) on top of the existing
    // spread that already pulls all three together as you climb.
    int getNumChoices() const override { return 2; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctLevelIndex; }

    // Numbers and units only, so it survives every language untouched -
    // see Game::getAnswerDetail. The teaching value of the old English
    // feedback was exactly these two figures; the words around them are
    // now the editor's to localise.
    juce::String getAnswerDetail() const override
    {
        const auto& preset = presets[(size_t) pairLevels[(size_t) correctLevelIndex]];
        return juce::String (preset.ratio, 0) + ":1 · "
                 + juce::String (preset.thresholdDb, 0) + " dB";
    }
    int getChosenChoiceIndex() const override { return chosenLevelIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    // See Game::getHintView.
    HintView getHintView() const override { return HintView::envelope; }

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

public:
    // One way of arriving at this much compression.
    //
    // "Medium" was a single threshold-and-ratio pair, which is one
    // recording of medium compression rather than the thing itself. What
    // separates two compressors both doing a medium job is almost never
    // the ratio - it is the attack and release: 30 ms of attack lets the
    // transient through and reads as punch, 3 ms catches it and reads as
    // control, at the same amount of gain reduction. That is the axis a
    // family here has to vary, and it is also the one worth learning.
    //
    // `archetypal` is 1 for the textbook example of its category and 0 for
    // the one sitting against a neighbour - see shared/PresetFamily.h.
    struct Variant
    {
        float thresholdOffsetDb = 0.0f;
        float ratioScale = 1.0f;
        float attackMs = 10.0f;
        float releaseMs = 150.0f;
        float archetypal = 1.0f;
    };

    static const std::vector<Variant>& familyFor (int level);

    // Test seam: the level compensation measured for one voicing, so a
    // test can assert loudness never answers the question.
    float measureMakeupForTest (int level, const Variant& variant) const;

private:
    struct Preset
    {
        const char* label = "";
        float thresholdDb;
        float ratio;
    };

    // Computed in setDifficulty from one ramped spread value - see there
    // for why this replaced three fixed tier tables. Medium is the anchor
    // and never moves; the other two close in on it as the level rises.
    std::array<Preset, numLevels> presets {{
        { "Weak",   -12.0f, 2.0f },
        { "Medium", -18.0f, 4.0f },
        { "Strong", -24.0f, 8.0f }
    }};

    void updateCompressor();

    juce::dsp::Compressor<float> compressor;
    TestSignalGenerator noise;
    double sampleRate = 44100.0;

    int samplesSinceBurstStart = 0;
    int attackSamples = 1;
    int decayTauSamples = 1;
    int burstPeriodSamples = 1;

    juce::Random random;

    // Redrawn every round - see newRound for why the presets are not used
    // exactly as written.
    float roundThresholdJitterDb = 0.0f;
    float roundRatioJitter = 0.0f;

    // Which voicing is playing, and the compensation measured for it -
    // both settled on the message thread in newRound(), so the audio
    // thread reads two plain values and never runs the measurement.
    Variant roundVariant;
    float roundMakeupGain = 1.0f;

    // The two amounts on offer this round, as indices into `presets`.
    std::array<int, 2> pairLevels { { 0, 2 } };
    int difficultyLevel = 1;

    std::array<int, 2> drawPair();

    // 0 = the two ends, 1 = adjacent. Used by tests to check that harder
    // levels really do offer neighbours more often.
    int correctLevelIndex = 0;
    int chosenLevelIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    // Read on the audio thread every block, written from the UI.
    std::atomic<bool> playProcessed { true };

    int correctCount = 0;
    int totalCount = 0;
};

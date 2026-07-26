#pragma once

#include "Game.h"
#include <atomic>
#include "../TestSignalGenerator.h"
#include <array>

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
    // Repeats a burst with silence between it already - see
    // Game::hasOwnRepeatPause.
    bool hasOwnRepeatPause() const override { return true; }

    void setDifficulty (int level) override;
    void setReferenceAudioLibrary (const ReferenceAudioLibrary* library) override { noise.setLibrary (library); }

    // A/B - comparing the treated signal against the untreated one is
    // how a change is actually heard; see Game::supportsBeforeAfter.
    bool supportsBeforeAfter() const override { return true; }
    void setPlayProcessed (bool shouldPlayProcessed) override { playProcessed.store (shouldPlayProcessed); }
    bool isPlayingProcessed() const override { return playProcessed.load(); }
    juce::String getBeforeLabel() const override { return "Comp Off"; }
    juce::String getAfterLabel() const override { return "Comp On"; }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    int getNumChoices() const override { return numLevels; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctLevelIndex; }
    int getChosenChoiceIndex() const override { return chosenLevelIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

private:
    struct Preset
    {
        const char* label;
        float thresholdDb;
        float ratio;
        // Fixed compensation tuned by ear so the three presets sit at
        // roughly equal perceived loudness; not computed from measured
        // gain reduction.
        float makeupGainDb;
    };

    // Three difficulty tiers, same labels throughout - only how far apart
    // the presets sit changes. Easy (levels 1-3) is the original wide
    // spread; medium/hard (4-6/7-10) converge the threshold/ratio values
    // (and shrink the makeup-gain compensation to match) so the character
    // difference between "Weak" and "Strong" gets progressively subtler.
    static const std::array<Preset, numLevels> easyPresets;
    static const std::array<Preset, numLevels> mediumPresets;
    static const std::array<Preset, numLevels> hardPresets;
    const std::array<Preset, numLevels>* activePresets = &easyPresets;

    void updateCompressor();

    juce::dsp::Compressor<float> compressor;
    TestSignalGenerator noise;
    double sampleRate = 44100.0;

    int samplesSinceBurstStart = 0;
    int attackSamples = 1;
    int decayTauSamples = 1;
    int burstPeriodSamples = 1;

    juce::Random random;

    int correctLevelIndex = 0;
    int chosenLevelIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    // Read on the audio thread every block, written from the UI.
    std::atomic<bool> playProcessed { true };

    int correctCount = 0;
    int totalCount = 0;
};

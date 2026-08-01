#pragma once

#include "Game.h"
#include <atomic>
#include "../../shared/TestSignalGenerator.h"
#include <array>
#include <vector>
#include "../../shared/PresetFamily.h"

// "Guess the distortion type" exercise: continuous pink noise driven into
// one of four fixed waveshaper types. Same labels throughout - what
// changes with setDifficulty is the drive amount (the pre-gain applied
// before waveshaping): high drive at easy levels makes each type's
// character obvious, low drive at hard levels keeps all four close to
// clean, same "same labels, converging character" shape as
// CompressionGame's preset tables.
class DistortionGame : public Game
{
public:
    static constexpr int numTypes = 4;
    enum class Type { softClip, hardClip, tapeSaturation, overdrive };

    juce::String getName() const override { return "Guess the Distortion"; }
    juce::String getInstructions() const override
    {
        return "Listen, then guess the type of distortion. Soft and hard "
               "clipping generate different harmonics - even-order ones "
               "read as warmer, odd-order as harsher and more aggressive.";
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
    juce::String getBeforeLabel() const override { return "Clean"; }
    juce::String getAfterLabel() const override { return "Driven"; }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    // **Always two** - see ReverbGame for why. More buttons is more
    // reading and more luck, not finer hearing; two alternatives makes the
    // question "which of these", and difficulty becomes how close together
    // they are.
    int getNumChoices() const override { return 2; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctTypeIndex; }
    int getChosenChoiceIndex() const override { return chosenTypeIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

public:
    // One voicing of a type. A family of these is what stops "Tape" being
    // a single recording you learn to recognise - see PresetFamily.h.
    //
    // The type still chooses the *curve*; a variant chooses how far along
    // that type's own character it sits. A tape with its rolloff pushed up
    // to 11 kHz is still tape, and it is also very nearly a soft clip,
    // which is exactly what a hard level should be able to ask.
    struct Variant
    {
        float driveScale = 1.0f;      // multiplies the level's drive amount
        float toneCutoffHz = 0.0f;    // 0 = no post-shaping rolloff
        float negativeScale = 1.0f;   // 1 = symmetric; below 1 = asymmetric knee
        float archetypal = 1.0f;
    };

    static const std::vector<Variant>& familyFor (int type);

    // Test seams. Both are pure functions over their arguments, so a test
    // can assert that loudness never becomes the giveaway by running the
    // real curve rather than a copy of it that could quietly drift.
    static float measureMakeupFor (Type type, const Variant& variant, float drive, double sampleRate);
    static float shape (Type type, float driven, float negativeScale);

private:
    struct TypeInfo
    {
        const char* label;
    };

    static const std::array<TypeInfo, numTypes> types;

    float waveshape (Type type, float driven) const;

    TestSignalGenerator noise;
    double sampleRate = 44100.0;
    float tapeLowpassState = 0.0f;
    float tapeLowpassCoeff = 0.4f;

    // Easy (1-3): high drive, obvious character. Medium (4-6) / hard
    // (7-10): progressively less drive, closer to clean, harder to tell
    // the curves apart.
    float driveAmount = 6.0f;

    // Redrawn every round - see newRound.
    float roundDriveJitter = 0.0f;

    // Which voicing of the correct type is playing, and the level
    // compensation measured for it. Both settled on the message thread in
    // newRound(); the audio thread only reads them.
    Variant roundVariant;
    float roundMakeup = 1.0f;

    juce::Random random;

    // The two categories on offer this round. correctTypeIndex is 0 or 1 into
    // this, not an index into the full list.
    std::array<int, 2> pairIndices { { 0, 1 } };
    int difficultyLevel = 1;

    // Where each shaper sits on a "how hard does it bite" axis. Soft clip
    // and tape are neighbours because both round the peak rather than
    // squaring it - that really is the pair people confuse, and tape only
    // separates by its dulled top. Hard clip is the outlier at the far end.
    static const std::vector<float>& axisPositions();

    int correctTypeIndex = 0;
    int chosenTypeIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    // Read on the audio thread every block, written from the UI.
    std::atomic<bool> playProcessed { true };

    int correctCount = 0;
    int totalCount = 0;
};

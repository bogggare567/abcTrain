#pragma once

#include "Game.h"
#include "../TestSignalGenerator.h"
#include <array>

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

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    int getNumChoices() const override { return numTypes; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctTypeIndex; }
    int getChosenChoiceIndex() const override { return chosenTypeIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

private:
    struct TypeInfo
    {
        const char* label;
        // Fixed compensation tuned by ear so all four sit at roughly
        // equal perceived loudness at any drive amount - not measured,
        // same approach as CompressionGame's makeupGainDb.
        float makeupGain;
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

    juce::Random random;

    int correctTypeIndex = 0;
    int chosenTypeIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;
};

#include "DistortionGame.h"
#include "../../shared/PinkNoiseGenerator.h"
#include <cmath>

const std::array<DistortionGame::TypeInfo, DistortionGame::numTypes> DistortionGame::types {{
    { "Soft Clipping"   },
    { "Hard Clipping"   },
    { "Tape Saturation" },
    { "Overdrive"       }
}};

const std::vector<DistortionGame::Variant>& DistortionGame::familyFor (int type)
{
    // Four voicings each, ordered here by how textbook they are. The last
    // one in every family is deliberately close to a neighbouring type -
    // a tape bright enough to nearly be a soft clip, an overdrive
    // symmetric enough to nearly be one too. Those only come out at the
    // higher levels, which is what "harder" means here.

    static const std::vector<Variant> softClip {
        { 1.00f,     0.0f, 1.00f, 1.00f },   // the textbook symmetric tanh
        { 1.45f,     0.0f, 1.00f, 0.80f },   // driven harder, more compressed peak
        { 0.70f,     0.0f, 1.00f, 0.55f },   // barely into the curve
        { 1.10f, 12000.0f, 1.00f, 0.25f }    // a touch of top off it - nearly tape
    };

    static const std::vector<Variant> hardClip {
        { 1.00f,    0.0f, 1.00f, 1.00f },    // squared off, the obvious one
        { 1.60f,    0.0f, 1.00f, 0.85f },    // slammed
        { 0.65f,    0.0f, 1.00f, 0.50f },    // only the loudest peaks flattened
        { 0.90f, 7000.0f, 1.00f, 0.20f }     // clipped then dulled - nearly tape
    };

    static const std::vector<Variant> tape {
        { 1.00f,  4000.0f, 1.00f, 1.00f },   // the reference machine
        { 1.20f,  2800.0f, 1.00f, 0.85f },   // darker, more obviously tape
        { 0.85f,  6500.0f, 1.00f, 0.45f },   // brighter, less of a tell
        { 1.00f, 11000.0f, 1.00f, 0.20f }    // so bright it is nearly a soft clip
    };

    static const std::vector<Variant> overdrive {
        { 1.00f, 0.0f, 0.60f, 1.00f },       // the asymmetric knee, plainly
        { 1.30f, 0.0f, 0.45f, 0.85f },       // more asymmetry, more even harmonics
        { 0.85f, 0.0f, 0.72f, 0.45f },       // gentler
        { 1.00f, 0.0f, 0.88f, 0.20f }        // nearly symmetric - nearly a soft clip
    };

    switch ((Type) type)
    {
        case Type::softClip:       return softClip;
        case Type::hardClip:       return hardClip;
        case Type::tapeSaturation: return tape;
        case Type::overdrive:      return overdrive;
    }

    return softClip;
}

namespace
{
    std::vector<PresetFamily::Weighted> weightsFor (const std::vector<DistortionGame::Variant>& family)
    {
        std::vector<PresetFamily::Weighted> weights;
        weights.reserve (family.size());

        for (size_t i = 0; i < family.size(); ++i)
            weights.push_back ({ (int) i, family[i].archetypal });

        return weights;
    }

    float onePoleCoeff (float cutoffHz, double sampleRate)
    {
        if (cutoffHz <= 0.0f || sampleRate <= 0.0)
            return 1.0f;   // 1 = pass straight through

        return 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * cutoffHz / (float) sampleRate);
    }
}

void DistortionGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // The post-shaping rolloff's coefficient is now per-variant, not per
    // type, so newRound() sets it - which prepare() calls next anyway.
    newRound();
}

float DistortionGame::shape (Type type, float driven, float negativeScale)
{
    switch (type)
    {
        case Type::hardClip:
            return juce::jlimit (-1.0f, 1.0f, driven);

        case Type::softClip:
        case Type::tapeSaturation:
        case Type::overdrive:
            // One curve for the three rounded types; what separates them
            // is the variant. An asymmetric knee (softer on the negative
            // half) is what gives overdrive its even-harmonic character,
            // and tape's rolloff is applied after this, in process().
            return driven >= 0.0f ? std::tanh (driven)
                                  : std::tanh (driven * negativeScale);
    }

    return driven;
}

float DistortionGame::waveshape (Type type, float driven) const
{
    return shape (type, driven, roundVariant.negativeScale);
}

float DistortionGame::measureMakeupFor (Type type, const Variant& variant, float drive, double sampleRate)
{
    // Measured, not guessed. The four types used to carry one hand-tuned
    // makeup gain each, which was near enough while every type had exactly
    // one voicing and one drive amount. It stops being near enough the
    // moment a family varies the drive: a hotter variant is a louder
    // variant, and then the answer is audible as level rather than as
    // character - the one thing every one of these exercises is built to
    // avoid.
    //
    // So: run a reproducible signal through this exact voicing and scale
    // it to a fixed RMS.
    //
    // **Pink** noise, because that is what the game plays. The first
    // version measured full-scale white noise - some 15 dB hotter than the
    // real signal - so it compensated for an amount of clipping that never
    // happens and left the treated side several dB off. A waveshaper is a
    // level-dependent device; measuring it at the wrong level measures a
    // different device.
    constexpr int numSamples = 65536;
    constexpr float targetRms = 0.20f;

    PinkNoiseGenerator measuringNoise { 0x5EED };
    const auto toneCoeff = onePoleCoeff (variant.toneCutoffHz, sampleRate);

    auto toneState = 0.0f;
    auto sum = 0.0;
    auto sumOfSquares = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto raw = measuringNoise.nextSample();
        auto shaped = shape (type, raw * drive, variant.negativeScale);

        if (variant.toneCutoffHz > 0.0f)
        {
            toneState += toneCoeff * (shaped - toneState);
            shaped = toneState;
        }

        sum += (double) shaped;
        sumOfSquares += (double) shaped * (double) shaped;
    }

    // Variance, not raw mean square: an asymmetric curve - which is
    // exactly what Overdrive is - produces a DC offset, and DC is not
    // loudness. Counting it made the compensation wander with whatever
    // offset the noise happened to produce, and left Overdrive the one
    // type that would not settle.
    const auto mean = sum / (double) numSamples;
    const auto variance = juce::jmax (0.0, sumOfSquares / (double) numSamples - mean * mean);
    const auto rms = (float) std::sqrt (variance);

    // A silent result would mean an infinite makeup. Can't happen with a
    // real curve, but the guard costs nothing and a NaN reaching the audio
    // thread costs a lot.
    if (! (rms > 1.0e-6f))
        return 1.0f;

    return juce::jlimit (0.05f, 20.0f, targetRms / rms);
}

void DistortionGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const auto type = (Type) pairIndices[(size_t) correctTypeIndex];
    const auto makeup = roundMakeup;

    const auto processed = playProcessed.load();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto raw = noise.nextSample();
        const auto driven = raw * juce::jmax (0.2f, driveAmount * roundVariant.driveScale + roundDriveJitter);
        auto shaped = waveshape (type, driven);

        // Applied by the variant rather than by the type: a soft clip with
        // its top rolled off is a real voicing, and it is the one that
        // makes "is this tape?" a genuine question.
        if (roundVariant.toneCutoffHz > 0.0f)
        {
            tapeLowpassState += tapeLowpassCoeff * (shaped - tapeLowpassState);
            shaped = tapeLowpassState;
        }

        // Each side scaled to the same level, so switching A/B isolates
        // the harmonic character rather than the volume.
        const auto value = processed ? shaped * makeup * 0.35f
                                     : raw * roundCleanGain * 0.35f;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);
    }
}

void DistortionGame::setDifficulty (int level)
{
    difficultyLevel = juce::jlimit (1, 10, level);

    // Ramped across all ten levels, like every other game: the drive falls
    // from an obvious 6x to a subtle 1.2x, where the four types have to be
    // told apart by the harmonics they add rather than by how loud the
    // distortion is.
    driveAmount = rampTolerance (level, 6.0f, 1.2f);
}

void DistortionGame::newRound()
{
    pairIndices = PresetFamily::drawPair (axisPositions(), difficultyLevel, random);
    correctTypeIndex = random.nextInt (2);

    // Three independent draws - the pair sets the question, the family
    // member sets how archetypal the example is. Same split, and the same
    // reason for it, as ReverbGame::newRound.
    const auto type = (Type) pairIndices[(size_t) correctTypeIndex];
    const auto& family = familyFor ((int) type);
    roundVariant = family[(size_t) PresetFamily::choose (weightsFor (family), difficultyLevel, random)];

    // A per-round nudge on the drive, for the same reason the other
    // preset-driven games have one: four fixed drive amounts become four
    // memorised recordings, and then the exercise is recognition rather
    // than listening. Scaled by the current drive so it shrinks as the
    // levels get harder and the margins get thinner.
    roundDriveJitter = (random.nextFloat() * 0.3f - 0.15f) * driveAmount;

    // Settled here, on the message thread, so the audio thread reads two
    // plain floats and never runs the measurement itself.
    const auto drive = juce::jmax (0.2f, driveAmount * roundVariant.driveScale + roundDriveJitter);
    roundMakeup = measureMakeupFor (type, roundVariant, drive, sampleRate);

    // The untreated signal measured on its own and brought to the same
    // target, rather than borrowing the shaped path's number.
    {
        constexpr int numSamples = 4096;
        constexpr float targetRms = 0.20f;

        PinkNoiseGenerator cleanNoise { 0x5EED };
        auto sumOfSquares = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto value = cleanNoise.nextSample();
            sumOfSquares += (double) value * (double) value;
        }

        const auto cleanRms = (float) std::sqrt (sumOfSquares / (double) numSamples);
        roundCleanGain = cleanRms > 1.0e-6f ? juce::jlimit (0.05f, 20.0f, targetRms / cleanRms)
                                            : 1.0f;
    }
    tapeLowpassCoeff = onePoleCoeff (roundVariant.toneCutoffHz, sampleRate);

    chosenTypeIndex = -1;
    answered = false;
    tapeLowpassState = 0.0f;
    sendChangeMessage();
}

void DistortionGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenTypeIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctTypeIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String DistortionGame::getChoiceLabel (int choiceIndex) const
{
    return types[(size_t) pairIndices[(size_t) juce::jlimit (0, 1, choiceIndex)]].label;
}

juce::String DistortionGame::getFeedbackText() const
{
    if (! answered)
        return {};

    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + types[(size_t) pairIndices[(size_t) correctTypeIndex]].label + ".";
}

const std::vector<float>& DistortionGame::axisPositions()
{
    // Soft clip, hard clip, tape, overdrive - in the order `types` holds
    // them. Soft and tape sit close because both round the peak instead of
    // squaring it; tape separates only by its dulled top, which is exactly
    // the distinction worth training. Hard clip is the outlier.
    static const std::vector<float> positions { 0.28f, 0.95f, 0.14f, 0.55f };
    return positions;
}

#include "CompressionGame.h"
#include "shared/audio/PinkNoiseGenerator.h"
#include <cmath>

void CompressionGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    compressor.prepare (sampleRate);
    compressor.reset();

    noise.setExerciseBed (LessonAudioBed::Bed::drumLoop, sampleRate);

    attackSamples = juce::jmax (1, (int) (sampleRate * 0.003));
    decayTauSamples = juce::jmax (1, (int) (sampleRate * 0.07));
    burstPeriodSamples = juce::jmax (1, (int) (sampleRate * 0.6));
    samplesSinceBurstStart = 0;

    newRound();
}

void CompressionGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const auto shapeNoise = noise.isPlayingNoise();
    const auto processed = playProcessed.load();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto value = noise.nextSample();

        // Pink noise is shaped into hits; the drum loop or a clip already
        // has its own.
        if (shapeNoise)
        {
            if (samplesSinceBurstStart >= burstPeriodSamples)
                samplesSinceBurstStart = 0;

            value *= samplesSinceBurstStart < attackSamples
                       ? (float) samplesSinceBurstStart / (float) attackSamples
                       : std::exp ((float) -(samplesSinceBurstStart - attackSamples) / (float) decayTauSamples);
            ++samplesSinceBurstStart;
        }

        // The engine is fed every sample either way, so switching A/B does
        // not restart its envelope from zero. Only the compressed path gets
        // the makeup: it is a per-round dry-over-wet ratio, and applying it
        // to the dry side would make that side louder by exactly what the
        // compressor took away.
        const auto gain = compressor.computeGain (value);

        if (processed)
            value *= gain * roundMakeupGain;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value * 0.6f);
    }
}

void CompressionGame::setDifficulty (int level)
{
    // The three presets are now interpolated toward each other from one
    // spread value rather than picked from three fixed tables, so every
    // level moves them - reaching level 5 used to be indistinguishable
    // from level 4.
    //
    // Medium is the anchor and never moves; Weak and Strong close in on
    // it. At level 1 the spread is the original -12/-18/-24 dB at 2:1 /
    // 4:1 / 8:1, which nobody can miss; at level 10 it is roughly
    // -17.4/-18/-18.7 dB at 3.6:1 / 4:1 / 4.4:1, where only the character
    // of the squeeze separates them.
    difficultyLevel = juce::jlimit (1, 10, level);

    const auto spread = rampTolerance (level, 1.0f, 0.11f);

    // No makeup column any more: it is measured per voicing in
    // newRound() rather than tabulated, since a family that varies the
    // attack varies how much gain reduction actually happens.
    struct Offsets { const char* label; float thresholdDb; float ratio; };

    const std::array<Offsets, numLevels> offsets {{
        { "Weak",    6.0f, -2.0f },
        { "Medium",  0.0f,  0.0f },
        { "Strong", -6.0f,  4.0f }
    }};

    for (size_t i = 0; i < presets.size(); ++i)
    {
        presets[i].label       = offsets[i].label;
        presets[i].thresholdDb = -18.0f + offsets[i].thresholdDb * spread;
        presets[i].ratio       = 4.0f   + offsets[i].ratio       * spread;
    }

    // The round in progress keeps whatever it was set up with; the next
    // newRound() picks the new values up.
}

const std::vector<CompressionGame::Variant>& CompressionGame::familyFor (int level)
{
    // Four voicings each, ordered here from textbook to borderline. The
    // last one in every family is deliberately reaching toward its
    // neighbour: a Weak setting leaning on the threshold, a Strong one
    // backed off it. Those only come out at the higher levels.
    //
    // Attack and release carry most of the difference, because that is
    // what actually differs between two compressors doing the same amount
    // of work. Threshold and ratio move only a little - move them far and
    // it stops being that category.

    static const std::vector<Variant> weak {
        {  0.0f, 1.00f, 20.0f, 200.0f, 1.00f },   // gentle and transparent
        {  0.0f, 1.00f, 45.0f, 400.0f, 0.80f },   // slower still: barely there
        { -2.0f, 1.15f, 12.0f, 140.0f, 0.50f },   // a little more grip
        { -4.0f, 1.30f,  6.0f, 110.0f, 0.20f }    // leaning toward Medium
    };

    static const std::vector<Variant> medium {
        {  0.0f, 1.00f, 10.0f, 150.0f, 1.00f },   // the reference
        {  0.0f, 1.00f, 30.0f, 120.0f, 0.85f },   // slow attack: punch
        { -1.0f, 1.10f,  4.0f, 300.0f, 0.50f },   // fast and smooth
        { +3.0f, 0.80f, 18.0f, 220.0f, 0.20f }    // backed off toward Weak
    };

    static const std::vector<Variant> strong {
        {  0.0f, 1.00f,  3.0f, 100.0f, 1.00f },   // obviously squashed
        {  0.0f, 1.00f,  1.0f,  60.0f, 0.85f },   // slammed
        { -2.0f, 1.20f, 15.0f, 250.0f, 0.50f },   // slower, higher ratio
        { +4.0f, 0.70f,  8.0f, 180.0f, 0.20f }    // backed off toward Medium
    };

    switch (juce::jlimit (0, numLevels - 1, level))
    {
        case 0:  return weak;
        case 1:  return medium;
        default: return strong;
    }
}

namespace
{
    std::vector<PresetFamily::Weighted> weightsFor (const std::vector<CompressionGame::Variant>& family)
    {
        std::vector<PresetFamily::Weighted> weights;
        weights.reserve (family.size());

        for (size_t i = 0; i < family.size(); ++i)
            weights.push_back ({ (int) i, family[i].archetypal });

        return weights;
    }
}

float CompressionGame::measureMakeupForTest (int level, const Variant& variant) const
{
    // Measured, not tuned by ear.
    //
    // The three settings used to carry one hand-picked makeup gain each,
    // which held while each was a single fixed voicing. A family that
    // varies the threshold, the ratio *and* the attack varies how much
    // gain reduction actually happens - and then the round is winnable by
    // hearing which one is quieter, which is a different and much easier
    // exercise. Same fix and the same reasoning as DistortionGame.
    //
    // Two details decide whether this works at all.
    //
    // It measures **pink** noise, not white. A compressor is a
    // level-dependent device, so calibrating on a signal ~10 dB hotter
    // than the one actually played gives a compensation for compression
    // that never happens - the first version did exactly that and left a
    // 5 dB spread across settings, which the test below caught.
    //
    // And it returns dry-over-wet rather than a fixed target level, so it
    // restores precisely what the compressor took away and the absolute
    // scale of the measurement signal cancels out.
    const auto rate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const auto numSamples = (int) (rate * 2.4);

    CompressorEngine measuring;
    measuring.prepare (rate);

    // Exactly what updateCompressor() is about to set on the real one,
    // *including this round's jitter*. Measuring the un-jittered setting
    // left a 2.8 dB spread, because a threshold nudged a dB either way
    // changes how much gain reduction happens and therefore how much
    // needs putting back.
    const auto& preset = presets[(size_t) juce::jlimit (0, numLevels - 1, level)];
    measuring.setParameters (preset.thresholdDb + variant.thresholdOffsetDb + roundThresholdJitterDb,
                             juce::jmax (1.05f, preset.ratio * variant.ratioScale + roundRatioJitter),
                             variant.attackMs, variant.releaseMs, kneeDb, 0.0f);

    // The material the player is about to hear - the drum loop, their own
    // clip, or noise shaped into the same hits the game plays. A
    // compressor is level- and envelope-dependent, so measuring it on
    // anything else measures a different device.
    juce::AudioBuffer<float> scratch (1, numSamples);
    noise.fillForMeasurement (scratch.getWritePointer (0), numSamples, 0x5EED);

    if (noise.isPlayingNoise())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto position = i % juce::jmax (1, burstPeriodSamples);
            const auto envelope = position < attackSamples
                                    ? (float) position / (float) juce::jmax (1, attackSamples)
                                    : std::exp ((float) -(position - attackSamples)
                                                    / (float) juce::jmax (1, decayTauSamples));

            scratch.setSample (0, i, scratch.getSample (0, i) * envelope);
        }
    }

    const auto rmsOf = [&scratch, numSamples]
    {
        auto sum = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const auto v = (double) scratch.getSample (0, i);
            sum += v * v;
        }
        return (float) std::sqrt (sum / (double) numSamples);
    };

    const auto dry = rmsOf();

    auto* data = scratch.getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
        data[i] *= measuring.computeGain (data[i]);

    const auto wet = rmsOf();

    if (! (wet > 1.0e-6f) || ! (dry > 1.0e-6f))
        return 1.0f;

    return juce::jlimit (0.05f, 20.0f, dry / wet);
}

void CompressionGame::newRound()
{
    // The pair sets the question; which of the two is the answer is a
    // separate draw, so a hard pair does not also bias which side is
    // right.
    pairLevels = drawPair();
    correctLevelIndex = drawCorrectOfPair (random, pairLevels[0], pairLevels[1]);
    noise.nextBedVariation (random);

    // A small random nudge on top of the preset, redrawn every round.
    //
    // Without it the three settings are three fixed sounds, and after a
    // few dozen rounds the exercise stops being "how hard is this being
    // squeezed" and becomes "which of these three recordings is it" -
    // which is a memory test wearing a listening test's clothes. The
    // jitter is smaller than the gap between neighbouring presets at every
    // level, so it makes the sound less memorable without making the
    // answer ambiguous.
    roundThresholdJitterDb = random.nextFloat() * 2.0f - 1.0f;
    roundRatioJitter = random.nextFloat() * 0.4f - 0.2f;

    // Which voicing of the correct setting is playing - drawn separately
    // from the pair, so a hard pair does not always arrive with a hard
    // example. Same three-independent-draws split as ReverbGame.
    const auto level = pairLevels[(size_t) correctLevelIndex];
    const auto& family = familyFor (level);
    roundVariant = family[(size_t) PresetFamily::choose (weightsFor (family), difficultyLevel, random)];
    roundMakeupGain = measureMakeupForTest (level, roundVariant);

    chosenLevelIndex = -1;
    answered = false;
    updateCompressor();
    sendChangeMessage();
}

void CompressionGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenLevelIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctLevelIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String CompressionGame::getChoiceLabel (int choiceIndex) const
{
    return presets[(size_t) pairLevels[(size_t) juce::jlimit (0, 1, choiceIndex)]].label;
}

juce::String CompressionGame::getFeedbackText() const
{
    if (! answered)
        return {};

    const auto& preset = presets[(size_t) pairLevels[(size_t) correctLevelIndex]];
    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + preset.label + " compression ("
           + juce::String (preset.ratio, 0) + ":1 at "
           + juce::String (preset.thresholdDb, 0) + " dB).";
}

void CompressionGame::updateCompressor()
{
    const auto& preset = presets[(size_t) pairLevels[(size_t) correctLevelIndex]];
    compressor.setParameters (preset.thresholdDb + roundVariant.thresholdOffsetDb + roundThresholdJitterDb,
                              juce::jmax (1.05f, preset.ratio * roundVariant.ratioScale + roundRatioJitter),
                              roundVariant.attackMs, roundVariant.releaseMs, kneeDb, 0.0f);
}

std::array<int, 2> CompressionGame::drawPair()
{
    // Three amounts means three pairs: the two ends, and the two
    // neighbouring pairs. Weak-against-Strong is the giveaway; either
    // neighbour pair is the real question.
    //
    // The chance of a neighbour pair climbs with the level rather than
    // switching over at a threshold, so no single level-up suddenly makes
    // the exercise feel like a different game. At level 1 it is one round
    // in six; at level 10 the easy pair is gone entirely.
    const auto neighbourChance = juce::jmap ((float) difficultyLevel, 1.0f, 10.0f, 0.17f, 1.0f);

    const auto wantNeighbours = random.nextFloat() < neighbourChance;

    auto pair = wantNeighbours
                    ? (random.nextBool() ? std::array<int, 2> { { 0, 1 } }
                                         : std::array<int, 2> { { 1, 2 } })
                    : std::array<int, 2> { { 0, 2 } };

    // Which side is offered first is random, or the louder answer would
    // always sit on the same end of the scale.
    if (random.nextBool())
        std::swap (pair[0], pair[1]);

    return pair;
}

Game::LevelMeaning CompressionGame::describeLevel (int level) const
{
    // Neighbouring amounts become the usual question past the middle of
    // the ladder (see drawPair's neighbourChance); before that the usual
    // one is the two ends.
    const auto neighbours = juce::jmap ((float) juce::jlimit (1, 10, level), 1.0f, 10.0f, 0.17f, 1.0f) >= 0.5f;
    LevelMeaning meaning;
    meaning.closerA = presets[neighbours ? 1u : 0u].label;
    meaning.closerB = presets[2].label;
    return meaning;
}

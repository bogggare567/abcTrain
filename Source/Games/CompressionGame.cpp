#include "CompressionGame.h"
#include <cmath>

void CompressionGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    compressor.prepare (spec);
    compressor.setAttack (5.0f);
    compressor.setRelease (120.0f);

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

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (samplesSinceBurstStart >= burstPeriodSamples)
            samplesSinceBurstStart = 0;

        float envelope;
        if (samplesSinceBurstStart < attackSamples)
            envelope = (float) samplesSinceBurstStart / (float) attackSamples;
        else
            envelope = std::exp ((float) -(samplesSinceBurstStart - attackSamples) / (float) decayTauSamples);

        const auto value = noise.nextSample() * envelope;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);

        ++samplesSinceBurstStart;
    }

    const auto& preset = presets[(size_t) pairLevels[(size_t) correctLevelIndex]];

    if (playProcessed.load())
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        compressor.process (context);

        buffer.applyGain (juce::Decibels::decibelsToGain (preset.makeupGainDb) * 0.6f);
    }
    else
    {
        // Uncompressed, but at the *same* makeup gain, so A/B isolates the
        // dynamics rather than turning into a loudness comparison - which
        // would be a different (and much easier) exercise.
        buffer.applyGain (juce::Decibels::decibelsToGain (preset.makeupGainDb) * 0.6f);
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

    struct Offsets { const char* label; float thresholdDb; float ratio; float makeupDb; };

    const std::array<Offsets, numLevels> offsets {{
        { "Weak",    6.0f, -2.0f, -4.0f },
        { "Medium",  0.0f,  0.0f,  0.0f },
        { "Strong", -6.0f,  4.0f,  4.0f }
    }};

    for (size_t i = 0; i < presets.size(); ++i)
    {
        presets[i].label        = offsets[i].label;
        presets[i].thresholdDb  = -18.0f + offsets[i].thresholdDb * spread;
        presets[i].ratio        = 4.0f   + offsets[i].ratio       * spread;
        presets[i].makeupGainDb = 6.0f   + offsets[i].makeupDb    * spread;
    }

    // The round in progress keeps whatever it was set up with; the next
    // newRound() picks the new values up.
}

void CompressionGame::newRound()
{
    // The pair sets the question; which of the two is the answer is a
    // separate draw, so a hard pair does not also bias which side is
    // right.
    pairLevels = drawPair();
    correctLevelIndex = random.nextInt (2);

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
    compressor.setThreshold (preset.thresholdDb + roundThresholdJitterDb);
    compressor.setRatio (juce::jmax (1.05f, preset.ratio + roundRatioJitter));
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

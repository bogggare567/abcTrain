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

    const auto& preset = presets[(size_t) correctLevelIndex];

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
    correctLevelIndex = random.nextInt (numLevels);

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
    return presets[(size_t) choiceIndex].label;
}

juce::String CompressionGame::getFeedbackText() const
{
    if (! answered)
        return {};

    const auto& preset = presets[(size_t) correctLevelIndex];
    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + preset.label + " compression ("
           + juce::String (preset.ratio, 0) + ":1 at "
           + juce::String (preset.thresholdDb, 0) + " dB).";
}

void CompressionGame::updateCompressor()
{
    const auto& preset = presets[(size_t) correctLevelIndex];
    compressor.setThreshold (preset.thresholdDb + roundThresholdJitterDb);
    compressor.setRatio (juce::jmax (1.05f, preset.ratio + roundRatioJitter));
}

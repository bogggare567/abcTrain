#include "DelayGame.h"
#include <cmath>
#include <limits>
#include <cmath>

const std::array<float, DelayGame::numDelayTimes> DelayGame::delayTimesMs { 50.0f, 150.0f, 300.0f, 500.0f };

float DelayGame::normalisedToMs (float normalised) noexcept
{
    return axisMinMs * std::pow (axisMaxMs / axisMinMs, juce::jlimit (0.0f, 1.0f, normalised));
}

float DelayGame::msToNormalised (float ms) noexcept
{
    return juce::jlimit (0.0f, 1.0f, std::log (ms / axisMinMs) / std::log (axisMaxMs / axisMinMs));
}

float DelayGame::getToleranceNormalised() const
{
    // A constant ratio is a constant *distance* on a log axis, so this is
    // just the ratio expressed in axis units.
    return std::log (1.0f + toleranceRatio) / std::log (axisMaxMs / axisMinMs);
}

juce::String DelayGame::formatNormalisedValue (float normalised) const
{
    return juce::String (juce::roundToInt (normalisedToMs (normalised))) + " ms";
}

std::vector<Game::GridMark> DelayGame::getGridMarks() const
{
    std::vector<GridMark> marks;

    // Doublings, plus their midpoints - the same two-density ruler shape
    // the frequency scale uses, for the same reason.
    static constexpr float emphasisedMs[] = { 20.0f, 40.0f, 80.0f, 160.0f, 320.0f, 640.0f };
    static constexpr float quietMs[]      = { 28.0f, 57.0f, 113.0f, 226.0f, 453.0f };

    for (const auto ms : emphasisedMs)
        marks.push_back ({ msToNormalised (ms), juce::String ((int) ms) + "ms", true });

    for (const auto ms : quietMs)
        marks.push_back ({ msToNormalised (ms), juce::String ((int) ms) + "ms", false });

    return marks;
}

void DelayGame::submitNormalisedAnswer (float normalised)
{
    if (answered)
        return;

    chosenNormalised = juce::jlimit (0.0f, 1.0f, normalised);
    chosenDelayIndex = -1;

    const auto guessedMs = normalisedToMs (chosenNormalised);
    const auto ratio = guessedMs > targetMs ? guessedMs / targetMs : targetMs / guessedMs;
    lastAnswerCorrect = (ratio - 1.0f) <= toleranceRatio;

    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

void DelayGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    delayLine.setMaximumDelayInSamples ((int) (sampleRate * 0.6) + 16);
    delayLine.prepare (spec);

    attackSamples = juce::jmax (1, (int) (sampleRate * 0.003));
    decayTauSamples = juce::jmax (1, (int) (sampleRate * 0.07));
    updateBurstPeriod();
    samplesSinceBurstStart = 0;

    newRound();
}

void DelayGame::process (juce::AudioBuffer<float>& buffer)
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

        const auto dry = noise.nextSample() * envelope;

        // The delay line is fed and read every block regardless, so its
        // state stays warm and switching back to "with echo" doesn't drop
        // the tail that was already in flight.
        delayLine.pushSample (0, dry);
        const auto wet = delayLine.popSample (0);
        const auto value = playProcessed.load()
                               ? dryWetFraction * wet + (1.0f - dryWetFraction) * dry
                               : dry;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);

        ++samplesSinceBurstStart;
    }
}

void DelayGame::setDifficulty (int level)
{
    // Two levers, both ramped: how isolated each burst is, and how wide
    // the accept band is. The band is a *ratio* rather than a number of
    // milliseconds - being 20 ms out at 40 ms and at 500 ms are completely
    // different mistakes.
    burstPeriodSeconds = rampLinear (level, 1.4f, 0.55f);
    toleranceRatio = rampTolerance (level, 0.35f, 0.08f);

    updateBurstPeriod();
}

void DelayGame::updateBurstPeriod()
{
    burstPeriodSamples = juce::jmax (1, (int) (sampleRate * burstPeriodSeconds));
}

void DelayGame::newRound()
{
    // Log-uniform, so short and long delays come up equally often;
    // rounded to 5 ms so the answer is always a nameable number.
    targetMs = juce::jlimit (axisMinMs, axisMaxMs,
                              5.0f * juce::roundToInt (normalisedToMs (random.nextFloat()) / 5.0f));

    // Nearest legacy fixed time, for the discrete path only.
    correctDelayIndex = 0;
    auto smallestDistance = std::numeric_limits<float>::max();
    for (int i = 0; i < numDelayTimes; ++i)
    {
        const auto distance = std::abs (std::log (delayTimesMs[(size_t) i] / targetMs));
        if (distance < smallestDistance)
        {
            smallestDistance = distance;
            correctDelayIndex = i;
        }
    }

    chosenNormalised = -1.0f;
    chosenDelayIndex = -1;
    answered = false;
    updateDelayTime();
    sendChangeMessage();
}

void DelayGame::updateDelayTime()
{
    const auto ms = targetMs;
    delayLine.setDelay ((float) (sampleRate * ms / 1000.0));
}

void DelayGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenDelayIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctDelayIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String DelayGame::getChoiceLabel (int choiceIndex) const
{
    return juce::String ((int) delayTimesMs[(size_t) choiceIndex]) + "ms";
}

juce::String DelayGame::getFeedbackText() const
{
    if (! answered)
        return {};

    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + juce::String (juce::roundToInt (targetMs)) + " ms.";
}

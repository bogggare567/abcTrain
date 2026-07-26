#include "FrequencyRangeGame.h"
#include <cmath>

namespace
{
    juce::String formatFrequency (float hz)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, 1) + "k";
        return juce::String ((int) hz);
    }
}

const std::array<FrequencyRangeGame::Range, FrequencyRangeGame::numRanges> FrequencyRangeGame::ranges {{
    { "Sub-bass",   20.0f,   60.0f },
    { "Bass",       60.0f,   250.0f },
    { "Low-mids",   250.0f,  500.0f },
    { "Mids",       500.0f,  2000.0f },
    { "High-mids",  2000.0f, 4000.0f },
    { "Presence",   4000.0f, 6000.0f },
    { "Air",        6000.0f, 20000.0f }
}};

void FrequencyRangeGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    peakFilter.prepare (spec);
    newRound();
}

void FrequencyRangeGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto value = noise.nextSample();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);
    }

    // A/B: "before" is the flat noise, "after" has the round's boost or
    // cut in it. Naming a range is by far the hardest of the nine
    // exercises to get a foothold in - without a reference you are being
    // asked to recognise an absolute, which nobody can do cold - so being
    // able to flip back to untreated is not a convenience here, it is the
    // difference between practising and guessing.
    if (playProcessed.load())
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        peakFilter.process (context);
    }
    else
    {
        // Still runs the filter on a scratch copy so its state stays
        // continuous - flipping back mid-tail must not click.
        peakFilter.reset();
    }

    buffer.applyGain (0.25f);
}

void FrequencyRangeGame::setDifficulty (int level)
{
    // Ramped across all ten levels rather than stepping twice: an
    // unmissable 10 dB at level 1 down to 2.5 dB at level 10, where the
    // range has to be recognised by colour rather than by how loud the
    // bump is.
    gainDb = rampTolerance (level, 10.0f, 2.5f);
}

void FrequencyRangeGame::newRound()
{
    correctRangeIndex = random.nextInt (numRanges);
    const auto& range = ranges[(size_t) correctRangeIndex];
    // Log-uniform pick within the range, matching how frequency is
    // perceived - a linear random pick would bias heavily toward the
    // top of each (especially the wide Bass/Air) range.
    const auto logLow = std::log (range.lowHz);
    const auto logHigh = std::log (range.highHz);
    correctFreqHz = std::exp (logLow + random.nextFloat() * (logHigh - logLow));

    isBoost = random.nextBool();
    chosenRangeIndex = -1;
    answered = false;
    updateFilter();
    sendChangeMessage();
}

void FrequencyRangeGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenRangeIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctRangeIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String FrequencyRangeGame::getChoiceLabel (int choiceIndex) const
{
    return ranges[(size_t) choiceIndex].label;
}

juce::String FrequencyRangeGame::getFeedbackText() const
{
    if (! answered)
        return {};

    const juce::String direction = isBoost ? "boosted" : "cut";
    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + direction + " at " + formatFrequency (correctFreqHz) + " Hz ("
           + ranges[(size_t) correctRangeIndex].label + ").";
}

void FrequencyRangeGame::updateFilter()
{
    const auto gain = juce::Decibels::decibelsToGain (isBoost ? gainDb : -gainDb);
    *peakFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, correctFreqHz, filterQ, gain);
}

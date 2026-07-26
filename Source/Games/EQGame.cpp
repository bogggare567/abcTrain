#include "EQGame.h"
#include <cmath>
#include <limits>

namespace
{
    juce::String formatFrequency (float hz)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, 1) + "k";
        return juce::String ((int) hz);
    }

}

float EQGame::normalisedToFrequency (float normalised) noexcept
{
    return axisLowHz * std::pow (axisHighHz / axisLowHz, juce::jlimit (0.0f, 1.0f, normalised));
}

float EQGame::frequencyToNormalised (float hz) noexcept
{
    return juce::jlimit (0.0f, 1.0f,
                          std::log (hz / axisLowHz) / std::log (axisHighHz / axisLowHz));
}

std::vector<Game::GridMark> EQGame::getGridMarks() const
{
    std::vector<GridMark> marks;

    for (int i = 0; i < numBands; ++i)
    {
        const auto centre = bandFrequenciesHz[(size_t) i];

        // Boundary below this centre - half an octave down.
        const auto boundary = centre * 0.70710678f;
        marks.push_back ({ frequencyToNormalised (boundary),
                           formatFrequency (boundary) + "Hz", false });

        marks.push_back ({ frequencyToNormalised (centre),
                           formatFrequency (centre) + "Hz", true });
    }

    // The boundary past the topmost centre closes the ruler off.
    const auto topBoundary = bandFrequenciesHz.back() * 1.41421356f;
    marks.push_back ({ frequencyToNormalised (topBoundary),
                       formatFrequency (topBoundary) + "Hz", false });

    return marks;
}

float EQGame::getToleranceNormalised() const
{
    // Octaves are the unit an ear actually works in, so the band is a
    // constant *ratio* wide wherever it sits - the same forgiveness at
    // 200 Hz as at 8 kHz, which a linear tolerance would not give.
    return toleranceOctaves / axisOctaves;
}

juce::String EQGame::formatNormalisedValue (float normalised) const
{
    const auto hz = normalisedToFrequency (normalised);

    // Whole Hz low down, one decimal in kHz above 1k: "425 Hz", "1.6k Hz".
    if (hz >= 1000.0f)
        return juce::String (hz / 1000.0f, 1) + "k Hz";

    return juce::String (juce::roundToInt (hz)) + " Hz";
}

const std::array<float, EQGame::numBands> EQGame::bandFrequenciesHz {
    100.0f, 200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f, 6400.0f, 12800.0f
};

void EQGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    peakFilter.prepare (spec);
    newRound();
}

void EQGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto value = noise.nextSample();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);
    }

    // "Before" is the same noise with the filter skipped, so switching
    // A/B changes exactly one thing - which is what makes the comparison
    // worth anything.
    if (playProcessed.load())
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        peakFilter.process (context);
    }

    buffer.applyGain (0.25f);
}

void EQGame::setDifficulty (int level)
{
    // Both levers ramp across all ten levels (see Game::rampTolerance):
    // the boost shrinks from an unmissable 9 dB to a subtle 2.5 dB, and
    // the accept band from a whole octave either side down to a fifth of
    // one. The band is the real lever - it is the same *ratio* of slack at
    // 200 Hz as at 8 kHz, which a fixed number of hertz would not be.
    gainDb = rampTolerance (level, 9.0f, 2.5f);
    toleranceOctaves = rampTolerance (level, 1.0f, 0.2f);
}

void EQGame::newRound()
{
    // Log-uniform across the whole range: uniform in *octaves*, so every
    // part of the spectrum comes up equally often. Drawing uniformly in
    // Hz instead would put nearly every target above 6 kHz.
    // Drawn across the labelled 100 Hz - 12.8 kHz range rather than the
    // full axis, so a target never lands in the half-octave margin at
    // either end where there'd be no room to answer past it.
    targetHz = bandFrequenciesHz.front()
                   * std::pow (bandFrequenciesHz.back() / bandFrequenciesHz.front(), random.nextFloat());

    // Nearest grid mark, for the legacy discrete path only.
    correctBandIndex = 0;
    auto smallestDistance = std::numeric_limits<float>::max();
    for (int i = 0; i < numBands; ++i)
    {
        const auto distance = std::abs (std::log (bandFrequenciesHz[(size_t) i] / targetHz));
        if (distance < smallestDistance)
        {
            smallestDistance = distance;
            correctBandIndex = i;
        }
    }

    isBoost = random.nextBool();
    chosenBandIndex = -1;
    chosenNormalised = -1.0f;
    answered = false;
    updateFilter();
    sendChangeMessage();
}

void EQGame::submitNormalisedAnswer (float normalised)
{
    if (answered)
        return;

    chosenNormalised = juce::jlimit (0.0f, 1.0f, normalised);
    chosenBandIndex = -1;

    lastAnswerCorrect = std::abs (chosenNormalised - getCorrectNormalised()) <= getToleranceNormalised();

    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

void EQGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    // Legacy discrete path: keeps its original exact-grid-match rule
    // rather than the tolerance band, so anything still answering by
    // index behaves exactly as it did before continuous mode existed.
    chosenBandIndex = choiceIndex;
    chosenNormalised = (float) choiceIndex / (float) (numBands - 1);
    lastAnswerCorrect = (choiceIndex == correctBandIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String EQGame::getChoiceLabel (int choiceIndex) const
{
    return formatFrequency (bandFrequenciesHz[(size_t) choiceIndex]) + " Hz";
}

juce::String EQGame::getFeedbackText() const
{
    if (! answered)
        return {};

    const juce::String direction = isBoost ? "boosted" : "cut";
    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + direction + " at " + formatFrequency (targetHz) + " Hz.";
}

void EQGame::updateFilter()
{
    const auto freq = targetHz;
    const auto gain = juce::Decibels::decibelsToGain (isBoost ? gainDb : -gainDb);
    *peakFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, freq, filterQ, gain);
}

#include "EQGame.h"
#include "../../shared/PinkNoiseGenerator.h"
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

        // Boundary below this centre - half an octave down. Skipped when
        // it would fall off the axis, which is what stops a label being
        // drawn half outside the panel at either end.
        const auto boundary = centre * 0.70710678f;
        if (boundary > axisLowHz)
            marks.push_back ({ frequencyToNormalised (boundary),
                               formatFrequency (boundary) + "Hz", false });

        marks.push_back ({ frequencyToNormalised (centre),
                           formatFrequency (centre) + "Hz", true });
    }

    // The boundary past the topmost centre closes the ruler off, when
    // there is room for it.
    const auto topBoundary = bandFrequenciesHz.back() * 1.41421356f;
    if (topBoundary < axisHighHz)
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
    // ISO octave centres, the ones every analyser and graphic EQ is
    // ruled in.
    31.5f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f,
    8000.0f, 16000.0f
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

        // Back to the level it came in at - see updateMatchGain.
        buffer.applyGain (matchGain);
    }

    buffer.applyGain (0.25f);
}

void EQGame::updateMatchGain()
{
    // Pink noise through this round's filter, offline, on the message
    // thread. Its own filter instance so the live one's state is never
    // disturbed mid-round, and a fixed seed so the same round always
    // gets the same compensation.
    const auto numSamples = (int) (sampleRate > 0.0 ? sampleRate : 44100.0);

    juce::dsp::IIR::Filter<float> measuring;
    measuring.coefficients = peakFilter.state;
    measuring.reset();

    PinkNoiseGenerator measuringNoise { 0x5EED };

    matchGain = GainMatch::measure (numSamples,
        [&measuringNoise] (juce::AudioBuffer<float>& buffer)
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, measuringNoise.nextSample());
        },
        [&measuring] (juce::AudioBuffer<float>& buffer)
        {
            auto* data = buffer.getWritePointer (0);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = measuring.processSample (data[i]);
        });
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
    // Drawn from the audible middle of the axis, not its full span - see
    // targetLowHz/targetHighHz for why the ruler is wider than the
    // question.
    targetHz = targetLowHz * std::pow (targetHighHz / targetLowHz, random.nextFloat());

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
    updateMatchGain();
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

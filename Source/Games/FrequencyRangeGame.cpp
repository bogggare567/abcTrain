#include "FrequencyRangeGame.h"
#include "../../shared/PinkNoiseGenerator.h"
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
    scratch.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
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

        buffer.applyGain (matchGain);
    }
    else
    {
        // Run the filter on a scratch copy and throw the result away, so
        // its state stays continuous while "before" is playing.
        //
        // This used to call peakFilter.reset() instead, which is the exact
        // opposite: it zeroed the state on every block, so flipping to
        // "after" started a filter with Q up to 2.4 from cold against
        // full-level noise - a resonant transient, i.e. precisely the thump
        // the comment claimed to be preventing. Nothing caught it because
        // the comment described the intended behaviour and the code below
        // it did something else.
        scratch.setSize (numChannels, numSamples, false, false, true);

        for (int ch = 0; ch < numChannels; ++ch)
            scratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> scratchBlock (scratch);
        juce::dsp::ProcessContextReplacing<float> scratchContext (scratchBlock);
        peakFilter.process (scratchContext);
    }

    buffer.applyGain (0.25f);
}

void FrequencyRangeGame::setDifficulty (int level)
{
    difficultyLevel = juce::jlimit (1, 10, level);

    // Ramped across all ten levels rather than stepping twice: an
    // unmissable 10 dB at level 1 down to 2.5 dB at level 10, where the
    // range has to be recognised by colour rather than by how loud the
    // bump is.
    gainDb = rampTolerance (level, 10.0f, 2.5f);
}

void FrequencyRangeGame::newRound()
{
    pairIndices = PresetFamily::drawPair (axisPositions(), difficultyLevel, random);
    correctRangeIndex = random.nextInt (2);
    const auto& range = ranges[(size_t) pairIndices[(size_t) correctRangeIndex]];
    // Log-uniform pick within the range, matching how frequency is
    // perceived - a linear random pick would bias heavily toward the
    // top of each (especially the wide Bass/Air) range.
    //
    // *How much* of the range it may land in is the level's business, and
    // this is the same idea PresetFamily::breadthForLevel writes down for
    // the games whose categories are discrete presets. A frequency in the
    // middle of Bass is the archetypal example of Bass; one sitting on the
    // boundary with Low-mids is the hard case. Drawing uniformly across
    // the whole range at every level meant level 1 could hand out that
    // hard case and level 10 could hand out the giveaway - noise on the
    // difficulty curve rather than difficulty.
    const auto logLow = std::log (range.lowHz);
    const auto logHigh = std::log (range.highHz);
    const auto logCentre = 0.5f * (logLow + logHigh);
    const auto breadth = PresetFamily::breadthForLevel (difficultyLevel);
    const auto halfSpan = 0.5f * (logHigh - logLow) * breadth;

    correctFreqHz = std::exp (logCentre + (random.nextFloat() * 2.0f - 1.0f) * halfSpan);

    // Q is the second family axis. A broad bump lifts the whole range and
    // is what "Bass" sounds like; a narrow one is a single tone that
    // happens to live there, which is a genuinely harder question and the
    // one an engineer hunting a resonance actually faces.
    filterQ = juce::jmap (breadth, 0.34f, 1.0f, 0.7f, 2.4f);

    isBoost = random.nextBool();
    chosenRangeIndex = -1;
    answered = false;
    updateFilter();
    updateMatchGain();
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
    return ranges[(size_t) pairIndices[(size_t) juce::jlimit (0, 1, choiceIndex)]].label;
}

juce::String FrequencyRangeGame::getFeedbackText() const
{
    if (! answered)
        return {};

    const juce::String direction = isBoost ? "boosted" : "cut";
    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + direction + " at " + formatFrequency (correctFreqHz) + " Hz ("
           + ranges[(size_t) pairIndices[(size_t) correctRangeIndex]].label + ").";
}

void FrequencyRangeGame::updateMatchGain()
{
    // Same approach as EQGame's: this round's filter run offline over
    // pink noise, on its own instance so the live one is untouched.
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

void FrequencyRangeGame::updateFilter()
{
    const auto gain = juce::Decibels::decibelsToGain (isBoost ? gainDb : -gainDb);
    *peakFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, correctFreqHz, filterQ, gain);
}

const std::vector<float>& FrequencyRangeGame::axisPositions()
{
    // Spectral order, evenly spaced. Neighbouring ranges share a boundary
    // and really are confusable - low-mids against mids is a genuine
    // question, sub-bass against air is not. Evenly spaced rather than
    // placed by actual frequency, because the ranges are already named in
    // perceptual steps rather than in equal hertz.
    static const std::vector<float> positions {
        0.0f, 1.0f / 6.0f, 2.0f / 6.0f, 0.5f, 4.0f / 6.0f, 5.0f / 6.0f, 1.0f
    };
    return positions;
}

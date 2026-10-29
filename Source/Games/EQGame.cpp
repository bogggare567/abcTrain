#include "EQGame.h"
#include "shared/audio/PinkNoiseGenerator.h"
#include "shared/dsp/EQCoefficients.h"
#include <cmath>
#include <limits>

namespace
{
    // With its unit, whole: "425 Hz", "5.2 kHz". It used to return "5.2k"
    // for callers to append " Hz" to, and "5.2k Hz" is a token the unit
    // localiser cannot see - the player read "5,2k Гц" beside a scale
    // labelled "5 кГц".
    juce::String formatFrequency (float hz)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, 1) + " kHz";
        return juce::String ((int) hz) + " Hz";
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
    // The grid every analyser and EQ is ruled in: 20, 50, 100, 200, 500,
    // 1k ... 20k labelled, and an unlabelled line at every whole step in
    // between (30, 40 ... 90, 300 ... 900). The lines crowd together at
    // the top of each decade, which is what makes a log scale *look* like
    // one.
    //
    // The ruler used to be labelled at the octave centres, 31.5 to 16 k,
    // with half-octave boundaries between them. Octaves are equally spaced
    // on a log axis, so the grid looked linear - "не логарифмическая" was
    // the author's word for it - and the last label, 16 kHz, made the scale
    // seem to stop there although it runs to 20.
    std::vector<GridMark> marks;

    const auto label = [] (float hz)
    {
        return hz >= 1000.0f ? juce::String (juce::roundToInt (hz / 1000.0f)) + "kHz"
                             : juce::String (juce::roundToInt (hz)) + "Hz";
    };

    for (const auto decade : { 10.0f, 100.0f, 1000.0f, 10000.0f })
    {
        for (int step = 1; step <= 9; ++step)
        {
            const auto hz = decade * (float) step;

            if (hz < axisLowHz - 0.5f || hz > axisHighHz + 0.5f)
                continue;

            const auto labelled = step == 1 || step == 2 || step == 5;
            marks.push_back ({ frequencyToNormalised (hz), labelled ? label (hz) : juce::String(), labelled });
        }
    }

    marks.push_back ({ 1.0f, label (axisHighHz), true });
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

    // Whole Hz low down, one decimal in kHz above 1k: "425 Hz", "1.6 kHz".
    if (hz >= 1000.0f)
        return juce::String (hz / 1000.0f, 1) + " kHz";

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
    // A boost needs something to boost - see TestSignalGenerator::Need.
    noise.setNeed (TestSignalGenerator::Need::spectrum);
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
    // This round's filter over the round's own material, offline, on the message
    // thread. Its own filter instance so the live one's state is never
    // disturbed mid-round, and a fixed seed so the same round always
    // gets the same compensation.
    const auto numSamples = (int) (sampleRate > 0.0 ? sampleRate : 44100.0);

    juce::dsp::IIR::Filter<float> measuring;
    measuring.coefficients = peakFilter.state;
    measuring.reset();

    // Measured on the material the round will actually play - a clip, or
    // the fallback noise - rather than always on pink noise. A boost at
    // 8 kHz is a large loudness change on a hi-hat and a small one on pink
    // noise; compensating for the noise left the hi-hat round answerable
    // by level alone, which is the thing this gain exists to prevent.
    matchGain = GainMatch::measure (numSamples,
        [this] (juce::AudioBuffer<float>& buffer)
        {
            noise.fillForMeasurement (buffer.getWritePointer (0), buffer.getNumSamples(), 0x5EED);
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
    difficultyLevel = juce::jlimit (1, 10, level);
    gainDb = rampTolerance (level, 9.0f, 2.5f);
    toleranceOctaves = toleranceForLevel (level);
}

void EQGame::newRound()
{
    // Log-uniform across the whole range: uniform in *octaves*, so every
    // part of the spectrum comes up equally often. Drawing uniformly in
    // Hz instead would put nearly every target above 6 kHz.
    // Drawn from the audible middle of the axis, not its full span - see
    // targetLowHz/targetHighHz for why the ruler is wider than the
    // question.
    //
    // Weighted by where this player misses (Game::setBucketWeights): each
    // named range is drawn with probability proportional to its width in
    // octaves times its weight, then a point log-uniformly inside it. With
    // every weight equal that is exactly the log-uniform draw across the
    // whole span; with weights it asks more often where the ear is weak.
    //
    // And only where the clip has something to boost: a target the clip
    // has no energy at is redrawn (SpectralContent.h - a 40 Hz bell on a
    // hi-hat loop changes nothing and was being scored anyway). Redrawing
    // rather than reshaping the spans keeps the weighting above exactly as
    // it was among the frequencies that remain. Pink noise has everything,
    // so with no clip the first draw always stands and seeded rounds on
    // noise are unchanged.
    const auto& content = noise.activeContent();

    struct Span { int bucket; float lowHz, highHz, octaves; };
    std::vector<Span> spans;
    auto total = 0.0f;

    for (int i = 0; i < FrequencyRangeGame::numRanges; ++i)
    {
        const auto& r = FrequencyRangeGame::ranges[(size_t) i];
        const auto low = juce::jmax (targetLowHz, r.lowHz);
        const auto high = juce::jmin (targetHighHz, r.highHz);

        if (high <= low)
            continue;

        const auto octaves = std::log2 (high / low);
        spans.push_back ({ i, low, high, octaves });
        total += octaves * bucketWeight (i);
    }

    for (int attempt = 0; attempt < 32; ++attempt)
    {
        auto roll = random.nextFloat() * total;
        auto chosen = spans.empty() ? Span { 0, targetLowHz, targetHighHz, 1.0f } : spans.back();

        for (const auto& span : spans)
        {
            roll -= span.octaves * bucketWeight (span.bucket);
            if (roll <= 0.0f)
            {
                chosen = span;
                break;
            }
        }

        targetHz = chosen.lowHz * std::pow (chosen.highHz / chosen.lowHz, random.nextFloat());

        if (content.isPresentAt (targetHz))
            break;
    }

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

    // Boosts first, cuts from step 4 (Game::cutChanceForLevel).
    isBoost = random.nextFloat() >= cutChanceForLevel (difficultyLevel);
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
    return formatFrequency (bandFrequenciesHz[(size_t) choiceIndex]);
}

juce::String EQGame::getFeedbackText() const
{
    if (! answered)
        return {};

    const juce::String direction = isBoost ? "boosted" : "cut";
    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + direction + " at " + formatFrequency (targetHz) + ".";
}

void EQGame::updateFilter()
{
    const auto freq = targetHz;
    // The matched bell (EQCoefficients::makeMatchedBell): the RBJ one this
    // replaced narrowed near Nyquist, which made the top targets quietly
    // harder than the level said.
    *peakFilter.state = EQCoefficients::makeArray (EQCoefficients::BandType::bell, sampleRate, freq,
                                                   isBoost ? gainDb : -gainDb, filterQ);
}

float EQGame::toleranceForLevel (int level) noexcept
{
    return rampTolerance (level, 1.0f, 0.2f);
}

Game::LevelMeaning EQGame::describeLevel (int level) const
{
    LevelMeaning meaning;
    meaning.unit = LevelMeaning::Unit::octaves;
    meaning.tolerance = toleranceForLevel (level) * 1.0f;
    return meaning;
}

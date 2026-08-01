#include "StereoWidthGame.h"

const std::array<const char*, StereoWidthGame::numWidths> StereoWidthGame::widthLabels {
    "Narrow", "Normal", "Wide", "Extra Wide"
};

const std::vector<StereoWidthGame::Variant>& StereoWidthGame::family()
{
    // Ordered by how textbook the result is. The plain one is the clearest
    // example of its width; the ones keeping more and more of the bottom
    // in mono sound progressively narrower than their own number says, so
    // they sit toward the neighbouring category - which is what makes them
    // the harder members and why the higher levels see them.
    static const std::vector<Variant> variants {
        { 0.0f,   1.00f },   // widened whole - the width you were told
        { 90.0f,  0.75f },   // sub kept centred, as most masters are
        { 150.0f, 0.50f },   // the usual working crossover
        { 300.0f, 0.22f }    // most of the body centred - reads narrower
    };

    return variants;
}

namespace
{
    std::vector<PresetFamily::Weighted> weightsFor (const std::vector<StereoWidthGame::Variant>& family)
    {
        std::vector<PresetFamily::Weighted> weights;
        weights.reserve (family.size());

        for (size_t i = 0; i < family.size(); ++i)
            weights.push_back ({ (int) i, family[i].archetypal });

        return weights;
    }
}

void StereoWidthGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    newRound();
}

void StereoWidthGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    // Never all the way to zero. At the narrow end the per-round jitter
    // could push the multiplier past 0, which collapses the side signal
    // entirely and makes the two channels identical - a stereo-width
    // exercise playing mono. Caught by the test that checks left and right
    // actually differ.
    const auto width = juce::jmax (0.05f, widths[(size_t) pairIndices[(size_t) correctWidthIndex]] + roundWidthJitter);
    const auto processed = playProcessed.load();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto left = noiseL.nextSample() * 0.5f;
        const auto right = noiseR.nextSample() * 0.5f;

        const auto mid = (left + right) * 0.5f;
        auto side = (left - right) * 0.5f;

        // A/B's "Mono": kill the side entirely and put the same mid in
        // both channels. Computed inside the loop from the same sources,
        // so flipping states never restarts or reseeds the noise.
        if (! processed)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, sample, mid);
            continue;
        }

        // Split the side signal and widen only the part above the
        // crossover, leaving the low half centred. A one-pole is plenty:
        // the point is that the bottom stays put, not that the transition
        // is surgical - and a steeper filter here would be a phase
        // problem, which is a different lesson.
        if (monoSplitCoeff > 0.0f)
        {
            sideLowStateL += monoSplitCoeff * (side - sideLowStateL);
            side = (side - sideLowStateL) * width;
        }
        else
        {
            side *= width;
        }

        const auto outL = (mid + side) * matchGain;
        const auto outR = (mid - side) * matchGain;

        if (numChannels > 0) buffer.setSample (0, sample, outL);
        if (numChannels > 1) buffer.setSample (1, sample, outR);
        for (int ch = 2; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, mid);
    }
}

void StereoWidthGame::setDifficulty (int level)
{
    difficultyLevel = juce::jlimit (1, 10, level);

    // The four named widths are computed from one spread value rather than
    // picked from three hard-coded tables, so every level is a real step.
    // At level 1 they are 0.10 / 0.60 / 1.00 / 1.60 - unmistakable. At
    // level 10 they close to 0.62 / 0.86 / 1.00 / 1.14, which takes real
    // listening to separate.
    //
    // Everything is a distance from Normal (1.0), scaled by that spread,
    // and the ratios between the four are preserved - so "Wide" is always
    // wider than "Normal", at every level, which a table per tier could
    // only guarantee by hand.
    const auto spread = rampTolerance (level, 1.0f, 0.24f);

    const std::array<float, numWidths> offsets { -0.9f, -0.4f, 0.0f, 0.6f };

    for (size_t i = 0; i < widths.size(); ++i)
        widths[i] = 1.0f + offsets[i] * spread;
}

void StereoWidthGame::newRound()
{
    pairIndices = PresetFamily::drawPair (axisPositions(), difficultyLevel, random);
    correctWidthIndex = random.nextInt (2);

    // Same reasoning as CompressionGame::newRound: four fixed widths
    // become four memorised recordings. Scaled by the current spread, so
    // the nudge shrinks along with the gaps it has to stay inside.
    roundWidthJitter = (random.nextFloat() * 0.16f - 0.08f)
                           * (widths[numWidths - 1] - widths[0]);

    // How the width is *arrived at*, drawn separately from which pair the
    // round asks about - the same three-independent-draws split as
    // ReverbGame::newRound, for the same reason.
    roundVariant = family()[(size_t) PresetFamily::choose (weightsFor (family()), difficultyLevel, random)];

    monoSplitCoeff = roundVariant.monoBelowHz > 0.0f && sampleRate > 0.0
                       ? 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                              * roundVariant.monoBelowHz / (float) sampleRate)
                       : 0.0f;
    sideLowStateL = 0.0f;

    updateMatchGain();

    chosenWidthIndex = -1;
    answered = false;
    sendChangeMessage();
}

void StereoWidthGame::updateMatchGain()
{
    // Mono against this round's width, measured on two fresh decorrelated
    // noise sources - the same shape the game plays, on its own
    // generators so the live ones keep their streams.
    //
    // Measured on the *sum* of the two channels, because that is what
    // loudness is here: widening pushes the two apart, which raises the
    // total even though the mid is untouched.
    const auto rate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const auto numSamples = (int) rate;
    const auto width = juce::jmax (0.05f, widths[(size_t) pairIndices[(size_t) correctWidthIndex]]
                                              + roundWidthJitter);
    const auto splitCoeff = monoSplitCoeff;

    PinkNoiseGenerator leftNoise { 0x5EED }, rightNoise { 0xBEEF };

    juce::AudioBuffer<float> mono (1, numSamples);
    juce::AudioBuffer<float> wide (1, numSamples);

    auto lowState = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto left = leftNoise.nextSample() * 0.5f;
        const auto right = rightNoise.nextSample() * 0.5f;

        const auto midValue = (left + right) * 0.5f;
        auto side = (left - right) * 0.5f;

        if (splitCoeff > 0.0f)
        {
            lowState += splitCoeff * (side - lowState);
            side = (side - lowState) * width;
        }
        else
        {
            side *= width;
        }

        // The two channels folded into one sample whose square is their
        // mean square, so a single RMS over the buffer is the RMS of the
        // pair. Summing them instead would measure the mid twice and the
        // side not at all, which is exactly the quantity that must not be
        // used here.
        mono.setSample (0, i, midValue);
        wide.setSample (0, i, std::sqrt (0.5f * ((midValue + side) * (midValue + side)
                                                  + (midValue - side) * (midValue - side))));
    }

    matchGain = GainMatch::from (GainMatch::rms (mono), GainMatch::rms (wide));
}

void StereoWidthGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenWidthIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctWidthIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String StereoWidthGame::getChoiceLabel (int choiceIndex) const
{
    return widthLabels[(size_t) pairIndices[(size_t) juce::jlimit (0, 1, choiceIndex)]];
}

juce::String StereoWidthGame::getFeedbackText() const
{
    if (! answered)
        return {};

    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + widthLabels[(size_t) pairIndices[(size_t) correctWidthIndex]] + ".";
}

const std::vector<float>& StereoWidthGame::axisPositions()
{
    // Width is already a single axis, so these are just the four steps
    // evenly placed. No interpretation is needed and none is invented.
    static const std::vector<float> positions { 0.0f, 0.33f, 0.66f, 1.0f };
    return positions;
}

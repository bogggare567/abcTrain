#include "ReverbGame.h"
#include "../../shared/PinkNoiseGenerator.h"
#include "../../shared/PresetFamily.h"
#include <cmath>

const std::array<float, 4> ReverbGame::springFrequenciesHz { 320.0f, 730.0f, 1400.0f, 2600.0f };
const std::array<const char*, ReverbGame::numTypes> ReverbGame::typeLabels { "Room", "Chamber", "Hall", "Plate", "Spring" };

void ReverbGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    reverb.prepare (spec);
    for (auto& allpass : springAllpass)
        allpass.prepare (spec);

    attackSamples = juce::jmax (1, (int) (sampleRate * 0.003));
    decayTauSamples = juce::jmax (1, (int) (sampleRate * 0.05));
    // Longer than CompressionGame's burst period - reverb tails need room
    // to decay audibly before the next hit.
    burstPeriodSamples = juce::jmax (1, (int) (sampleRate * 1.2));
    samplesSinceBurstStart = 0;

    newRound();
}

void ReverbGame::process (juce::AudioBuffer<float>& buffer)
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

    // A/B: "Dry" is the same burst with the space taken away, not a
    // different signal. The dry path skips the effect entirely rather
    // than feeding it silently - the moment you flip back you want the
    // reverb to start from the hit you are hearing, not to dump a tail
    // it accumulated while supposedly off.
    if (playProcessed.load())
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);

        if (pairTypes[(size_t) correctTypeIndex] == springTypeIndex)
        {
            for (auto& allpass : springAllpass)
                allpass.process (context);
        }
        else
        {
            reverb.process (context);
        }

        buffer.applyGain (matchGain);
    }

    buffer.applyGain (0.7f);
}

void ReverbGame::setDifficulty (int level)
{
    // The count no longer moves - it is always two. What a level changes
    // is which *pairs* may be drawn and how borderline an example may be.
    difficultyLevel = juce::jlimit (1, 10, level);
}

std::vector<PresetFamily::Weighted> ReverbGame::weightsFor (const std::vector<Variant>& family)
{
    std::vector<PresetFamily::Weighted> weights;
    weights.reserve (family.size());

    for (int i = 0; i < (int) family.size(); ++i)
        weights.push_back ({ i, family[(size_t) i].archetypal });

    return weights;
}

float ReverbGame::confusabilityOf (int typeA, int typeB)
{
    // Where each type sits on a rough "how much like a big natural room"
    // axis.
    //
    //   Room 0.15 · Chamber 0.5 · Hall 0.9 · Plate 0.55
    //
    // Chamber and Plate landing close together is not an accident: a
    // damped plate and a bright chamber really are the pair people get
    // wrong, and this is where that fact is written down.
    static const std::array<float, numTypes> position { { 0.15f, 0.5f, 0.9f, 0.55f, 0.35f } };

    const auto a = juce::jlimit (0, numTypes - 1, typeA);
    const auto b = juce::jlimit (0, numTypes - 1, typeB);

    // Spring is not on that axis at all: its character is a *mechanism*,
    // not a size, so the gap between two position numbers does not
    // describe it. Against a room, a chamber or a hall it is unmistakable.
    // Against a plate it is a genuine question - both are metal being
    // excited rather than air in a space, and telling a tank from a sheet
    // is one of the few reverb distinctions worth real practice.
    //
    // This used to be a blanket "any pair with Spring in it is at least
    // 0.75 apart", which made Spring maximally far from *everything*
    // including Plate - so it filled the easy levels and then vanished
    // from every hard one. See ADR 031.
    if (a == springTypeIndex || b == springTypeIndex)
    {
        const auto other = (a == springTypeIndex) ? b : a;

        if (other == springTypeIndex)
            return 0.0f;

        return other == plateTypeIndex ? 0.30f : 0.85f;
    }

    return juce::jlimit (0.0f, 1.0f,
                          std::abs (position[(size_t) a] - position[(size_t) b]));
}

std::array<int, 2> ReverbGame::drawPair()
{
    // The shared rule, with this game's own idea of what "far apart"
    // means passed in - see confusabilityOf. Keeping a private copy of
    // the selection logic here was how the two drifted apart the first
    // time; there is one implementation now.
    static const std::vector<float> positions { 0.15f, 0.5f, 0.9f, 0.55f, 0.35f };

    return PresetFamily::drawPair (positions, difficultyLevel, random,
                                    [] (int a, int b) { return confusabilityOf (a, b); });
}

void ReverbGame::newRound()
{
    // Draw the pair first, then which of the two is the answer, then
    // which member of that type's family is playing. Three independent
    // draws rather than one: the pair sets the *question*, the family
    // member sets how archetypal the example is, and conflating them
    // would make a hard pair always come with a hard example, which is
    // twice as hard as intended and impossible to reason about.
    pairTypes = drawPair();
    correctTypeIndex = random.nextInt (2);

    const auto& family = familyFor (pairTypes[(size_t) correctTypeIndex]);
    roundVariant = family[(size_t) PresetFamily::choose (weightsFor (family), difficultyLevel, random)];

    chosenTypeIndex = -1;
    answered = false;

    reverb.reset();
    for (auto& allpass : springAllpass)
        allpass.reset();

    updateReverbForType();
    updateMatchGain();
    sendChangeMessage();
}

void ReverbGame::updateMatchGain()
{
    // This round's space, run offline over the game's own burst shape on
    // separate DSP instances so the live tail is never disturbed. Three
    // burst periods, so the measurement sees the tails rather than only
    // the hits that cause them.
    const auto rate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const auto numSamples = juce::jmax (1, burstPeriodSamples * 5);

    // The first two periods are processed but not measured. This reverb
    // starts from silence while the live one has been running for as long
    // as the round has, and measuring through the build-up reports the wet
    // path as several dB quieter than it really is.
    const auto warmUp = juce::jmax (1, burstPeriodSamples * 2);
    const auto isSpring = pairTypes[(size_t) correctTypeIndex] == springTypeIndex;

    // Two channels, matching what process() hands the reverb.
    juce::dsp::ProcessSpec spec { rate, (juce::uint32) numSamples, 2 };

    juce::dsp::Reverb measuringReverb;
    measuringReverb.prepare (spec);
    measuringReverb.setParameters (reverb.getParameters());

    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                               juce::dsp::IIR::Coefficients<float>>, 4> measuringSpring;

    for (size_t i = 0; i < measuringSpring.size(); ++i)
    {
        measuringSpring[i].prepare (spec);
        *measuringSpring[i].state = *springAllpass[i].state;
    }

    PinkNoiseGenerator measuringNoise { 0x5EED };
    const auto attack = juce::jmax (1, attackSamples);
    const auto decay = juce::jmax (1, decayTauSamples);
    const auto period = juce::jmax (1, burstPeriodSamples);

    matchGain = GainMatch::measure (2, numSamples, warmUp,
        [&measuringNoise, attack, decay, period] (juce::AudioBuffer<float>& buffer)
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto position = i % period;
                const auto envelope = position < attack
                                        ? (float) position / (float) attack
                                        : std::exp ((float) -(position - attack) / (float) decay);

                // The same value in both channels, exactly as the game
                // renders it.
                const auto value = measuringNoise.nextSample() * envelope;

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.setSample (ch, i, value);
            }
        },
        [&measuringReverb, &measuringSpring, isSpring] (juce::AudioBuffer<float>& buffer)
        {
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> context (block);

            if (isSpring)
                for (auto& allpass : measuringSpring)
                    allpass.process (context);
            else
                measuringReverb.process (context);
        });
}

void ReverbGame::submitAnswer (int choiceIndex)
{
    if (answered)
        return;

    chosenTypeIndex = choiceIndex;
    lastAnswerCorrect = (choiceIndex == correctTypeIndex);
    answered = true;
    ++totalCount;
    if (lastAnswerCorrect)
        ++correctCount;

    sendChangeMessage();
}

juce::String ReverbGame::getChoiceLabel (int choiceIndex) const
{
    if (choiceIndex < 0 || choiceIndex >= 2)
        return {};

    return typeLabels[(size_t) pairTypes[(size_t) choiceIndex]];
}

juce::String ReverbGame::getFeedbackText() const
{
    if (! answered)
        return {};

    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + juce::String (typeLabels[(size_t) pairTypes[(size_t) correctTypeIndex]]) + " reverb.";
}

const std::vector<ReverbGame::Variant>& ReverbGame::familyFor (int type)
{
    // Every number here is tuned by ear rather than measured - the same
    // "approximation, not a physical model" precedent ADR 004 records for
    // LearnerVerb's own decay mapping. What matters is that the members of
    // a family are recognisably the same *kind* of space while being
    // audibly different rooms, and that the low-archetypal ones really do
    // sit close to a neighbour.

    static const std::vector<Variant> room {
        { 0.16f, 0.62f, 0.42f, 0.30f, 1.00f },   // tiled booth: tight, dead, narrow
        { 0.24f, 0.50f, 0.60f, 0.34f, 0.85f },   // wooden studio room
        { 0.30f, 0.40f, 0.70f, 0.36f, 0.55f },   // big live room - starting to be a chamber
        { 0.38f, 0.44f, 0.76f, 0.38f, 0.25f },   // borderline: nearly a chamber
    };

    static const std::vector<Variant> chamber {
        { 0.55f, 0.45f, 0.80f, 0.36f, 1.00f },   // the textbook chamber
        { 0.48f, 0.52f, 0.74f, 0.34f, 0.70f },   // smaller, darker
        { 0.62f, 0.38f, 0.86f, 0.38f, 0.45f },   // larger, brighter - leaning hall
        { 0.68f, 0.34f, 0.90f, 0.38f, 0.20f },   // borderline: nearly a small hall
    };

    static const std::vector<Variant> hall {
        { 0.95f, 0.22f, 1.00f, 0.40f, 1.00f },   // cathedral-scale
        { 0.88f, 0.26f, 0.96f, 0.38f, 0.80f },   // concert hall
        { 0.78f, 0.32f, 0.92f, 0.36f, 0.50f },   // small hall
        { 0.70f, 0.36f, 0.88f, 0.35f, 0.22f },   // borderline: nearly a chamber
    };

    static const std::vector<Variant> plate {
        { 0.50f, 0.04f, 1.00f, 0.36f, 1.00f },   // bright, dense, no room cue at all
        { 0.44f, 0.10f, 0.96f, 0.34f, 0.78f },   // a darker plate
        { 0.58f, 0.14f, 1.00f, 0.36f, 0.48f },   // longer, softer top
        { 0.52f, 0.22f, 0.94f, 0.35f, 0.20f },   // borderline: damped enough to read as a hall
    };

    // Spring is generated by the allpass cascade rather than by the
    // Freeverb parameters, so its family varies the wet level and width
    // only - the character comes from the filters.
    static const std::vector<Variant> spring {
        { 0.0f, 0.0f, 0.60f, 0.42f, 1.00f },
        { 0.0f, 0.0f, 0.45f, 0.36f, 0.70f },
        { 0.0f, 0.0f, 0.75f, 0.32f, 0.40f },
    };

    switch (type)
    {
        case 0:  return room;
        case 1:  return chamber;
        case 2:  return hall;
        case 3:  return plate;
        default: return spring;
    }
}

void ReverbGame::updateReverbForType()
{
    juce::dsp::Reverb::Parameters params;
    params.dryLevel = 0.0f;
    params.wetLevel = roundVariant.wet;
    params.roomSize = juce::jlimit (0.05f, 1.0f, roundVariant.roomSize);
    params.damping  = juce::jlimit (0.0f, 1.0f, roundVariant.damping);
    params.width    = juce::jlimit (0.0f, 1.0f, roundVariant.width);

    reverb.setParameters (params);

    for (int i = 0; i < (int) springAllpass.size(); ++i)
    {
        const auto freq = springFrequenciesHz[(size_t) i];
        *springAllpass[(size_t) i].state = *juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, freq, springQ);
    }
}

float ReverbGame::confusabilityForTest (const juce::String& labelA, const juce::String& labelB)
{
    const auto indexOf = [] (const juce::String& label)
    {
        for (int i = 0; i < numTypes; ++i)
            if (label == typeLabels[(size_t) i])
                return i;

        return 0;
    };

    return confusabilityOf (indexOf (labelA), indexOf (labelB));
}

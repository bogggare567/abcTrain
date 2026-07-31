#include "ReverbGame.h"
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
    // axis. Plate and Spring are off it - they are characters rather than
    // geometries - so they are placed by how easily they are mistaken for
    // something rather than by size.
    //
    //   Room 0.15 · Chamber 0.5 · Hall 0.9 · Plate 0.55 · Spring 0.35
    //
    // Chamber and Plate landing close together is not an accident: a
    // damped plate and a bright chamber really are the pair people get
    // wrong, and this is where that fact is written down.
    static const std::array<float, numTypes> position { { 0.15f, 0.5f, 0.9f, 0.55f, 0.35f } };

    const auto a = position[(size_t) juce::jlimit (0, numTypes - 1, typeA)];
    const auto b = position[(size_t) juce::jlimit (0, numTypes - 1, typeB)];

    // Spring's clang is unmistakable however close its position sits, so
    // any pair containing it is easier than the distance suggests.
    const auto involvesSpring = typeA == springTypeIndex || typeB == springTypeIndex;
    const auto distance = std::abs (a - b);

    return juce::jlimit (0.0f, 1.0f, involvesSpring ? juce::jmax (0.75f, distance) : distance);
}

std::array<int, 2> ReverbGame::drawPair()
{
    // Every unordered pair, with how far apart it is.
    struct Candidate { int a, b; float distance; };
    std::vector<Candidate> candidates;

    for (int a = 0; a < numTypes; ++a)
        for (int b = a + 1; b < numTypes; ++b)
            candidates.push_back ({ a, b, confusabilityOf (a, b) });

    // A level admits pairs no *easier* than its floor - so the hard tiers
    // stop offering cathedral-against-booth - and never harder than its
    // ceiling. Both move down together as the level rises.
    const auto ceiling = juce::jmap ((float) difficultyLevel, 1.0f, 10.0f, 1.0f, 0.30f);
    const auto floor = juce::jmap ((float) difficultyLevel, 1.0f, 10.0f, 0.55f, 0.0f);

    std::vector<Candidate> allowed;
    for (const auto& candidate : candidates)
        if (candidate.distance <= ceiling && candidate.distance >= floor)
            allowed.push_back (candidate);

    // A window that admits nothing would be a round that cannot happen;
    // fall back to the whole set rather than to a fixed pair, so the
    // failure is boring rather than repetitive.
    if (allowed.empty())
        allowed = candidates;

    const auto& picked = allowed[(size_t) random.nextInt ((int) allowed.size())];

    // Which of the two is offered first is itself random, or the answer
    // would drift towards one side of the scale.
    return random.nextBool() ? std::array<int, 2> { { picked.a, picked.b } }
                             : std::array<int, 2> { { picked.b, picked.a } };
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
    sendChangeMessage();
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

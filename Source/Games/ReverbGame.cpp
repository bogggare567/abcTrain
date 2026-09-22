#include "ReverbGame.h"
#include "shared/audio/PinkNoiseGenerator.h"
#include "shared/audio/PresetFamily.h"
#include <cmath>

const std::array<const char*, ReverbGame::numTypes> ReverbGame::typeLabels { "Room", "Chamber", "Hall", "Plate", "Spring" };

void ReverbGame::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    engine.prepare ({ sampleRate, spec.maximumBlockSize, 2 });
    // Longer than any block a host will hand over, so process() never has
    // to grow it on the audio thread.
    wetScratch.setSize (2, (int) juce::jmax<juce::uint32> (spec.maximumBlockSize, 8192));

    // A single hit and then the silence its tail lives in.
    noise.setExerciseBed (LessonAudioBed::Bed::singleHit, sampleRate);

    attackSamples = juce::jmax (1, (int) (sampleRate * 0.003));
    decayTauSamples = juce::jmax (1, (int) (sampleRate * 0.05));
    // Long enough for a hall's tail to decay audibly before the next hit.
    burstPeriodSamples = juce::jmax (1, (int) (sampleRate * 2.4));
    samplesSinceBurstStart = 0;

    newRound();
}

void ReverbGame::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const auto shapeNoise = noise.isPlayingNoise();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto value = noise.nextSample();

        // Pink noise is shaped into hits here; a bed or a clip already has
        // its own rhythm.
        if (shapeNoise)
        {
            if (samplesSinceBurstStart >= burstPeriodSamples)
                samplesSinceBurstStart = 0;

            const auto envelope = samplesSinceBurstStart < attackSamples
                                    ? (float) samplesSinceBurstStart / (float) attackSamples
                                    : std::exp ((float) -(samplesSinceBurstStart - attackSamples) / (float) decayTauSamples);
            value *= envelope;
            ++samplesSinceBurstStart;
        }

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, value);
    }

    // A/B: "Dry" is the same hit with the space taken away. The engine is
    // fed only while the space is on, so flipping back to Wet starts the
    // tail from the hit you are hearing rather than dumping one it
    // accumulated while supposedly off.
    if (playProcessed.load() && numChannels > 0)
    {
        // In pieces no longer than the scratch buffer, so a host handing
        // over a longer block than it announced still gets the space and
        // still allocates nothing.
        const auto chunk = wetScratch.getNumSamples();

        for (int start = 0; start < numSamples; start += chunk)
        {
            const auto length = juce::jmin (chunk, numSamples - start);

            for (int ch = 0; ch < 2; ++ch)
                wetScratch.copyFrom (ch, 0, buffer, juce::jmin (ch, numChannels - 1), start, length);

            juce::dsp::AudioBlock<float> wetBlock (wetScratch.getArrayOfWritePointers(), 2, (size_t) length);
            engine.process (wetBlock);

            // A send, the way a reverb is used: the dry hit plus the space
            // at this voicing's level, then back to the dry path's loudness.
            for (int ch = 0; ch < numChannels; ++ch)
            {
                buffer.addFrom (ch, start, wetScratch, juce::jmin (ch, 1), 0, length, roundVariant.send);
                buffer.applyGain (ch, start, length, matchGain);
            }
        }
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

const std::vector<float>& ReverbGame::characterPositions()
{
    static const std::vector<float> positions { 0.15f, 0.5f, 0.9f, 0.55f, 0.35f };
    return positions;
}

std::array<int, 2> ReverbGame::drawPair()
{
    // The shared rule, with this game's own idea of what "far apart"
    // means passed in - see confusabilityOf. Keeping a private copy of
    // the selection logic here was how the two drifted apart the first
    // time; there is one implementation now.
    return PresetFamily::drawPair (characterPositions(), difficultyLevel, random,
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
    correctTypeIndex = drawCorrectOfPair (random, pairTypes[0], pairTypes[1]);
    noise.nextBedVariation (random);

    const auto& family = familyFor (pairTypes[(size_t) correctTypeIndex]);
    roundVariant = family[(size_t) PresetFamily::choose (weightsFor (family), difficultyLevel, random)];

    chosenTypeIndex = -1;
    answered = false;

    updateReverbForType();
    updateMatchGain();
    sendChangeMessage();
}

void ReverbGame::updateMatchGain()
{
    // This round's space, run offline over what the player is about to
    // hear, on its own engine so the live tail is never disturbed. Several
    // hit periods, so the measurement sees the tails and not only the hits.
    const auto rate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const auto period = juce::jmax (1, burstPeriodSamples);
    const auto numSamples = period * 4;

    // The first period is processed but not measured: the measuring engine
    // starts from silence while the live one has been running all round,
    // and measuring through the build-up reports the wet path quieter than
    // it is.
    const auto warmUp = period;

    ReverbEngine measuring;
    measuring.prepare ({ rate, (juce::uint32) numSamples, 2 });
    measuring.setTypeNow (roundVariant.engine);
    measuring.setParameters (roundVariant.engine, roundVariant.decaySeconds, roundVariant.preDelayMs,
                             roundVariant.size, roundVariant.damping, roundVariant.width);

    std::vector<float> source ((size_t) numSamples);
    noise.fillForMeasurement (source.data(), numSamples, 0x5EED);

    if (noise.isPlayingNoise())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto position = i % period;
            source[(size_t) i] *= position < attackSamples
                                    ? (float) position / (float) juce::jmax (1, attackSamples)
                                    : std::exp ((float) -(position - attackSamples) / (float) juce::jmax (1, decayTauSamples));
        }
    }

    const auto send = roundVariant.send;

    matchGain = GainMatch::measure (2, numSamples, warmUp,
        [&source] (juce::AudioBuffer<float>& buffer)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.copyFrom (ch, 0, source.data(), buffer.getNumSamples());
        },
        [&measuring, send] (juce::AudioBuffer<float>& buffer)
        {
            juce::AudioBuffer<float> wet;
            wet.makeCopyOf (buffer);
            juce::dsp::AudioBlock<float> block (wet);
            measuring.process (block);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.addFrom (ch, 0, wet, ch, 0, buffer.getNumSamples(), send);
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
    // Real units, from what these spaces measure and what engineers set:
    // a vocal booth or small room 0.3-0.8 s with early reflections close
    // and loud; an echo chamber - a hard-walled room built for reverb -
    // 1-2 s and dense; a concert hall 1.8-3 s with its first reflections
    // far off; a plate 1.2-3 s, dense from the first instant and bright; a
    // spring 1-2.5 s with its drip. Every family's last member leans toward
    // its neighbour, and only comes out at the higher levels.
    using T = ReverbEngine::Type;
    //                              decay  pre   size  damp  width send  archetypal
    static const std::vector<Variant> room {
        { T::room,  0.35f,  0.0f, 0.20f, 0.60f, 0.60f, 0.45f, 1.00f },   // tight booth
        { T::room,  0.55f,  4.0f, 0.35f, 0.50f, 0.75f, 0.45f, 0.85f },   // wooden studio room
        { T::room,  0.75f,  6.0f, 0.50f, 0.45f, 0.80f, 0.42f, 0.50f },   // big live room
        { T::room,  0.95f,  8.0f, 0.62f, 0.40f, 0.85f, 0.40f, 0.20f },   // nearly a chamber
    };

    static const std::vector<Variant> chamber {
        { T::room,  1.40f, 10.0f, 0.85f, 0.35f, 0.85f, 0.40f, 1.00f },   // the textbook chamber
        { T::room,  1.15f,  8.0f, 0.75f, 0.45f, 0.80f, 0.40f, 0.70f },   // smaller, darker
        { T::room,  1.70f, 12.0f, 0.95f, 0.30f, 0.90f, 0.38f, 0.45f },   // larger, brighter
        { T::hall,  1.60f, 12.0f, 0.25f, 0.40f, 0.90f, 0.38f, 0.20f },   // nearly a small hall
    };

    static const std::vector<Variant> hall {
        { T::hall,  2.60f, 25.0f, 0.75f, 0.35f, 1.00f, 0.36f, 1.00f },   // concert hall
        { T::hall,  3.40f, 30.0f, 0.95f, 0.30f, 1.00f, 0.34f, 0.80f },   // very large hall
        { T::hall,  2.00f, 18.0f, 0.50f, 0.40f, 0.95f, 0.36f, 0.50f },   // small hall
        { T::hall,  1.80f, 14.0f, 0.35f, 0.45f, 0.90f, 0.38f, 0.22f },   // nearly a chamber
    };

    static const std::vector<Variant> plate {
        { T::plate, 1.80f,  0.0f, 0.55f, 0.08f, 1.00f, 0.34f, 1.00f },   // bright, dense, no walls
        { T::plate, 1.40f, 10.0f, 0.45f, 0.25f, 0.95f, 0.34f, 0.78f },   // a darker plate
        { T::plate, 2.60f,  5.0f, 0.70f, 0.20f, 1.00f, 0.32f, 0.48f },   // long, softer top
        { T::plate, 2.20f, 20.0f, 0.60f, 0.50f, 0.90f, 0.33f, 0.20f },   // damped enough to read as a hall
    };

    static const std::vector<Variant> spring {
        { T::spring, 1.80f, 0.0f, 0.50f, 0.20f, 0.60f, 0.40f, 1.00f },   // amp-style tank
        { T::spring, 1.20f, 0.0f, 0.35f, 0.30f, 0.50f, 0.38f, 0.70f },   // short spring
        { T::spring, 2.40f, 0.0f, 0.70f, 0.15f, 0.70f, 0.34f, 0.40f },   // long, splashy
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
    // Called between rounds: the space changes at once, from an empty tank.
    engine.setTypeNow (roundVariant.engine);
    engine.setParameters (roundVariant.engine, roundVariant.decaySeconds, roundVariant.preDelayMs,
                          roundVariant.size, roundVariant.damping, roundVariant.width);
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

Game::LevelMeaning ReverbGame::describeLevel (int level) const
{
    const auto pair = PresetFamily::hardestPairForLevel (characterPositions(), level,
                                                          [] (int a, int b) { return confusabilityOf (a, b); });
    LevelMeaning meaning;
    meaning.closerA = typeLabels[(size_t) pair[0]];
    meaning.closerB = typeLabels[(size_t) pair[1]];
    return meaning;
}

#include "ReverbGame.h"
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

    if (typeForSlot (correctTypeIndex) == springTypeIndex)
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
    // Which types are in play, and in an order chosen so each new one is
    // harder to separate from what is already there rather than just
    // "another option".
    //
    //   1-2   Room / Hall            - small against large
    //   3-4   + Plate                - a character that is not a room
    //   5-7   + Chamber              - now size alone stops working, because
    //                                  Chamber sits between the first two
    //   8-10  + Spring               - all five
    //
    // This is still the only game where difficulty changes the choice
    // count, which is why PluginEditor::refreshFromGameState has to cope
    // with it changing mid-session (see ADR 002).
    activeNumTypes = level <= 2 ? 2
                   : level <= 4 ? 3
                   : level <= 7 ? 4
                                : numTypes;
}

void ReverbGame::newRound()
{
    // Everything the interface touches - the correct index, the chosen
    // index, getChoiceLabel - is in *slot* space: 0..activeNumTypes-1, in
    // unlock order. Only the DSP translates a slot to a type through
    // typeOrder. Keeping both in one space was the first version, and it
    // silently marked correct answers wrong as soon as the two orders
    // diverged.
    correctTypeIndex = random.nextInt (activeNumTypes);

    roundSizeJitter = random.nextFloat() * 0.1f - 0.05f;
    roundDampingJitter = random.nextFloat() * 0.12f - 0.06f;
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
    if (choiceIndex < 0 || choiceIndex >= activeNumTypes)
        return {};

    return typeLabels[(size_t) typeForSlot (choiceIndex)];
}

juce::String ReverbGame::getFeedbackText() const
{
    if (! answered)
        return {};

    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + juce::String (typeLabels[(size_t) typeForSlot (correctTypeIndex)]) + " reverb.";
}

void ReverbGame::updateReverbForType()
{
    juce::dsp::Reverb::Parameters params;
    params.dryLevel = 0.0f;
    params.wetLevel = 0.35f;

    switch (typeForSlot (correctTypeIndex))
    {
        case 0: // Room: small, fairly damped, narrow
            params.roomSize = 0.22f;
            params.damping = 0.55f;
            params.width = 0.55f;
            break;
        case 1: // Chamber: medium and darker than a hall - a real room,
                // but a big one. Sits between Room and Hall on purpose.
            params.roomSize = 0.55f;
            params.damping = 0.45f;
            params.width = 0.8f;
            break;
        case 2: // Hall: large, bright, wide tail
            params.roomSize = 0.9f;
            params.damping = 0.25f;
            params.width = 1.0f;
            break;
        case 3: // Plate: dense and bright, not tied to a physical room size
            params.roomSize = 0.5f;
            params.damping = 0.08f;
            params.width = 1.0f;
            break;
        default: // Spring - reverb object unused, allpass cascade handles it
            break;
    }

    // Same reasoning again: without a nudge, "Hall" is one recording and
    // the exercise degenerates into recognising it rather than hearing
    // what a hall does. Small enough that no type wanders into another's
    // territory.
    params.roomSize = juce::jlimit (0.05f, 1.0f, params.roomSize + roundSizeJitter);
    params.damping  = juce::jlimit (0.0f, 1.0f, params.damping + roundDampingJitter);

    reverb.setParameters (params);

    for (int i = 0; i < (int) springAllpass.size(); ++i)
    {
        const auto freq = springFrequenciesHz[(size_t) i];
        *springAllpass[(size_t) i].state = *juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, freq, springQ);
    }
}

#include "ReverbGame.h"
#include <cmath>

const std::array<float, 4> ReverbGame::springFrequenciesHz { 320.0f, 730.0f, 1400.0f, 2600.0f };
const std::array<const char*, ReverbGame::numTypes> ReverbGame::typeLabels { "Room", "Hall", "Plate", "Spring" };

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

    if (correctTypeIndex == springTypeIndex)
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

void ReverbGame::newRound()
{
    correctTypeIndex = random.nextInt (numTypes);
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
    return typeLabels[(size_t) choiceIndex];
}

juce::String ReverbGame::getFeedbackText() const
{
    if (! answered)
        return {};

    return (lastAnswerCorrect ? juce::String ("Correct! ") : juce::String ("Not quite. "))
           + "It was " + juce::String (typeLabels[(size_t) correctTypeIndex]) + " reverb.";
}

void ReverbGame::updateReverbForType()
{
    juce::dsp::Reverb::Parameters params;
    params.dryLevel = 0.0f;
    params.wetLevel = 0.35f;

    switch (correctTypeIndex)
    {
        case 0: // Room: small, fairly damped
            params.roomSize = 0.25f;
            params.damping = 0.5f;
            params.width = 0.6f;
            break;
        case 1: // Hall: large, bright, wide tail
            params.roomSize = 0.85f;
            params.damping = 0.3f;
            params.width = 1.0f;
            break;
        case 2: // Plate: dense and bright, not tied to a physical room size
            params.roomSize = 0.5f;
            params.damping = 0.1f;
            params.width = 1.0f;
            break;
        default: // Spring - reverb object unused, allpass cascade handles it
            break;
    }

    reverb.setParameters (params);

    for (int i = 0; i < (int) springAllpass.size(); ++i)
    {
        const auto freq = springFrequenciesHz[(size_t) i];
        *springAllpass[(size_t) i].state = *juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, freq, springQ);
    }
}

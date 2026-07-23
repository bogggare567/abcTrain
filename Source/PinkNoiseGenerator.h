#pragma once

#include <juce_core/juce_core.h>

// Paul Kellet's "economy" pink noise algorithm (musicdsp.org).
class PinkNoiseGenerator
{
public:
    float nextSample()
    {
        const float white = random.nextFloat() * 2.0f - 1.0f;

        state[0] = 0.99886f * state[0] + white * 0.0555179f;
        state[1] = 0.99332f * state[1] + white * 0.0750759f;
        state[2] = 0.96900f * state[2] + white * 0.1538520f;
        state[3] = 0.86650f * state[3] + white * 0.3104856f;
        state[4] = 0.55000f * state[4] + white * 0.5329522f;
        state[5] = -0.7616f * state[5] - white * 0.0168980f;

        const float pink = state[0] + state[1] + state[2] + state[3]
                          + state[4] + state[5] + state[6] + white * 0.5362f;
        state[6] = white * 0.115926f;

        return pink * 0.11f;
    }

private:
    juce::Random random;
    float state[7] = {};
};

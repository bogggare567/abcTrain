#pragma once

#include <juce_core/juce_core.h>

// The three noise colours worth training on, and why there is a choice.
//
// Pink is the default and stays the default: equal energy per octave means
// a boost at 200 Hz and a boost at 8 kHz are equally loud, so a frequency
// question is fair wherever the answer lands. That is exactly what makes
// it the right *default* and a poor *only* option - it is also the least
// like anything you will ever mix.
//
//   pink   flat per octave. Fair. The reference.
//   white  flat per hertz, so it rises 3 dB per octave to the ear and the
//          top half of the spectrum dominates. Sibilance and air are far
//          easier to place in it; the bottom is nearly inaudible.
//   brown  falls 6 dB per octave. The opposite trade: low-mid problems
//          stand out, the top is gone. Closest of the three to the weight
//          of real music.
//
// Hearing the same exercise against all three is itself the lesson - the
// same 3 dB bump is obvious in one and nearly invisible in another, which
// is why "it sounded fine on my speakers" happens.
enum class NoiseColour { pink, white, brown };

// Paul Kellet's "economy" pink noise algorithm (musicdsp.org), plus the
// two neighbours either side of it.
class PinkNoiseGenerator
{
public:
    PinkNoiseGenerator() = default;

    void setColour (NoiseColour newColour) noexcept { colour = newColour; }
    NoiseColour getColour() const noexcept { return colour; }

    // Seeded, for the offline measurements that level one round's
    // processing against its own untreated signal. Those have to give the
    // same answer every time they are asked about the same setting -
    // otherwise the compensation wanders from round to round, which is a
    // level difference of its own.
    explicit PinkNoiseGenerator (juce::int64 seed) { random.setSeed (seed); }

    float nextSample()
    {
        const float white = random.nextFloat() * 2.0f - 1.0f;

        if (colour == NoiseColour::white)
        {
            // Scaled to land at roughly the same RMS as the pink path, so
            // switching colour is not also a volume change - the one thing
            // every exercise here is built to prevent.
            return white * 0.338f;
        }

        if (colour == NoiseColour::brown)
        {
            // A leaky integrator: integrating white noise is what brown
            // noise *is*. The leak keeps it from wandering off into DC,
            // which an ideal integrator would do within seconds.
            brownState = 0.997f * brownState + white * 0.045f;
            return brownState * 0.576f;
        }

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
    NoiseColour colour = NoiseColour::pink;
    float state[7] = {};
    float brownState = 0.0f;
};

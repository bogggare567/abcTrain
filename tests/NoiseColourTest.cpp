#include <juce_dsp/juce_dsp.h>
#include <cmath>

#include "../shared/PinkNoiseGenerator.h"

// The three noise colours have to be genuinely different in *tilt* and
// genuinely the same in *level*.
//
// Different, because the whole reason to offer a choice is that the same
// 3 dB bump is obvious in one and nearly invisible in another - which is
// the lesson behind "it sounded fine on my speakers". Two colours that
// measured the same would be one colour with two names.
//
// The same level, because everything in this product is a comparison, and
// a colour switch that is also a volume switch is the exact failure the
// gain-match work exists to prevent.
class NoiseColourTest : public juce::UnitTest
{
public:
    NoiseColourTest() : juce::UnitTest ("Noise colours", "Games") {}

    void runTest() override
    {
        beginTest ("each colour tilts the way its name says");
        {
            // Measured as the ratio of high-band to low-band energy. The
            // absolute figures depend on the filter shapes; the *ordering*
            // is the claim, and it is the one that matters.
            const auto pink  = tiltOf (NoiseColour::pink);
            const auto white = tiltOf (NoiseColour::white);
            const auto brown = tiltOf (NoiseColour::brown);

            logMessage ("high/low energy ratio - brown " + juce::String (brown, 4)
                         + ", pink " + juce::String (pink, 4)
                         + ", white " + juce::String (white, 4));

            expect (brown < pink,
                     "brown is not darker than pink (" + juce::String (brown, 4)
                         + " vs " + juce::String (pink, 4) + ")");
            expect (pink < white,
                     "white is not brighter than pink (" + juce::String (white, 4)
                         + " vs " + juce::String (pink, 4) + ")");

            // ...and by a margin worth hearing, not a rounding difference.
            expect (white / juce::jmax (1.0e-9f, brown) > 4.0f,
                     "the three colours are too close together to be three choices");
        }

        beginTest ("switching colour is not also switching volume");
        {
            const auto pink  = rmsOf (NoiseColour::pink);
            const auto white = rmsOf (NoiseColour::white);
            const auto brown = rmsOf (NoiseColour::brown);

            for (const auto& entry : { std::pair<const char*, float> { "white", white },
                                       std::pair<const char*, float> { "brown", brown } })
            {
                // Signed on purpose. An absolute value tells you the
                // level is wrong and not which way, which is how the first
                // correction here went the wrong way and doubled the gap.
                const auto differenceDb = juce::Decibels::gainToDecibels (entry.second / pink);

                // The same 1.5 dB the A/B gain-match test uses, and for the
                // same reason: below roughly 1 dB a level difference stops
                // being reliably audible at all.
                expect (std::abs (differenceDb) < 1.5f,
                         juce::String (entry.first) + " is " + juce::String (differenceDb, 2)
                             + " dB from pink (positive = louder)");
            }
        }

        beginTest ("brown does not wander off into DC");
        {
            // Integrating white noise is what brown noise is, and an ideal
            // integrator walks away from zero within seconds - which would
            // arrive as a DC offset in the player's output rather than as
            // audio. The leak in the integrator is what stops it; this is
            // the check that the leak is actually there.
            PinkNoiseGenerator generator;
            generator.setColour (NoiseColour::brown);

            auto sum = 0.0;
            constexpr int numSamples = 44100 * 20;

            for (int i = 0; i < numSamples; ++i)
                sum += (double) generator.nextSample();

            const auto mean = std::abs (sum / (double) numSamples);
            expect (mean < 0.02, "brown noise drifted to a DC offset of " + juce::String (mean, 4));
        }
    }

private:
    static constexpr int measuredSamples = 1 << 16;

    static std::vector<float> render (NoiseColour colour)
    {
        PinkNoiseGenerator generator { 0x11CE };
        generator.setColour (colour);

        std::vector<float> samples ((size_t) measuredSamples);

        for (auto& sample : samples)
            sample = generator.nextSample();

        return samples;
    }

    static float rmsOf (NoiseColour colour)
    {
        const auto samples = render (colour);
        auto sum = 0.0;

        for (const auto sample : samples)
            sum += (double) sample * (double) sample;

        return (float) std::sqrt (sum / (double) samples.size());
    }

    // Energy above 2 kHz over energy below 500 Hz. One FFT of the whole
    // render, which is enough: this is a question about a long-term
    // average, not about any one frame.
    static float tiltOf (NoiseColour colour)
    {
        constexpr int order = 14;
        constexpr int size = 1 << order;

        auto samples = render (colour);
        samples.resize ((size_t) size * 2, 0.0f);

        juce::dsp::WindowingFunction<float> window ((size_t) size,
                                                     juce::dsp::WindowingFunction<float>::hann);
        window.multiplyWithWindowingTable (samples.data(), (size_t) size);

        juce::dsp::FFT fft (order);
        fft.performFrequencyOnlyForwardTransform (samples.data());

        auto low = 0.0, high = 0.0;

        for (int bin = 1; bin < size / 2; ++bin)
        {
            const auto hz = (double) bin * 44100.0 / (double) size;
            const auto magnitude = (double) samples[(size_t) bin];

            if (hz < 500.0)        low += magnitude * magnitude;
            else if (hz > 2000.0)  high += magnitude * magnitude;
        }

        return (float) (high / juce::jmax (1.0e-12, low));
    }
};

static NoiseColourTest noiseColourTest;

#include <juce_audio_basics/juce_audio_basics.h>
#include "shared/dsp/CompressorEngine.h"
#include <chrono>

// The compressor checked the way the review in notes/ asked: measured, not
// assumed.
//  - the static curve: a steady level comes out where the formula says;
//  - attack and release: the envelope reaches 63 % in the time on the knob;
//  - the cost: gain -> dB -> curve -> dB -> gain runs every sample. The
//    review suspected the log/exp; this times it.
class CompressorCheckTest : public juce::UnitTest
{
public:
    CompressorCheckTest() : juce::UnitTest ("CompressorCheck", "DSP") {}

    void runTest() override
    {
        constexpr double fs = 48000.0;

        beginTest ("a steady level settles where the gain computer says (hard and soft knee)");
        for (float knee : { 0.0f, 6.0f })
            for (float levelDb : { -30.0f, -21.0f, -18.0f, -15.0f, -6.0f, 0.0f })
            {
                CompressorEngine c;
                c.prepare (fs);
                c.setParameters (-18.0f, 4.0f, 10.0f, 100.0f, knee, 0.0f);

                const auto x = juce::Decibels::decibelsToGain (levelDb);
                float gain = 1.0f;
                for (int i = 0; i < (int) fs; ++i)
                    gain = c.computeGain (x);

                const auto outDb = levelDb + juce::Decibels::gainToDecibels (gain);
                const auto want = levelDb - CompressorEngine::staticReductionDb (levelDb, -18.0f, 4.0f, knee);
                expectWithinAbsoluteError (outDb, want, 0.01f,
                                           "knee " + juce::String (knee) + " dB, in " + juce::String (levelDb) + " dB");
            }

        beginTest ("attack and release are the times on the knobs (63 % of the step, in dB)");
        for (float attackMs : { 1.0f, 10.0f, 30.0f })
            for (float releaseMs : { 50.0f, 300.0f })
            {
                CompressorEngine c;
                c.prepare (fs);
                c.setParameters (-30.0f, 10.0f, attackMs, releaseMs, 0.0f, 0.0f);

                const auto loud = juce::Decibels::decibelsToGain (-6.0f);
                const auto full = CompressorEngine::staticReductionDb (-6.0f, -30.0f, 10.0f, 0.0f);

                int attackSamples = -1;
                for (int i = 0; i < (int) fs && attackSamples < 0; ++i)
                {
                    c.computeGain (loud);
                    if (c.getLastGainReductionDb() >= 0.632f * full)
                        attackSamples = i + 1;
                }
                for (int i = 0; i < (int) fs; ++i)
                    c.computeGain (loud);

                int releaseSamples = -1;
                for (int i = 0; i < (int) fs * 2 && releaseSamples < 0; ++i)
                {
                    c.computeGain (0.0f);
                    if (c.getLastGainReductionDb() <= (1.0f - 0.632f) * full)
                        releaseSamples = i + 1;
                }

                expectWithinAbsoluteError (1000.0f * (float) attackSamples / (float) fs, attackMs, attackMs * 0.05f + 0.05f, "attack");
                expectWithinAbsoluteError (1000.0f * (float) releaseSamples / (float) fs, releaseMs, releaseMs * 0.05f, "release");
            }

        beginTest ("cost per sample (the review's question): report, and a generous ceiling");
        {
            CompressorEngine c;
            c.prepare (fs);
            c.setParameters (-18.0f, 4.0f, 10.0f, 100.0f, 6.0f, 0.0f);

            juce::Random random (7);
            std::vector<float> signal (1 << 16);
            for (auto& v : signal)
                v = random.nextFloat() * 2.0f - 1.0f;

            const int passes = 60;
            float sink = 0.0f;
            const auto start = std::chrono::steady_clock::now();
            for (int p = 0; p < passes; ++p)
                for (auto v : signal)
                    sink += c.computeGain (v);
            const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();

            const auto nsPerSample = seconds * 1.0e9 / (double) (passes * (int) signal.size());
            const auto shareOfCore = nsPerSample * fs * 1.0e-9 * 100.0;   // one detector per stereo pair (linked)
            logMessage (juce::String::formatted ("computeGain: %.1f ns per sample = %.3f %% of one core at 48 kHz (sink %g)",
                                                 nsPerSample, shareOfCore, (double) sink));

            // Unoptimised or under a sanitizer this can be tens of times slower,
            // so the ceiling only catches something pathological.
            expect (nsPerSample < 500.0, "computeGain takes " + juce::String (nsPerSample, 1) + " ns per sample");
        }
    }
};

static CompressorCheckTest compressorCheckTest;

#include <juce_dsp/juce_dsp.h>
#include <complex>

#include "shared/dsp/EQCoefficients.h"
#include "shared/audio/TestSignalGenerator.h"
#include "Source/Games/DistortionGame.h"
#include "Source/Games/EQGame.h"
#include "Source/Games/PanGame.h"
#include "Source/Games/FrequencyRangeGame.h"

// ADR 040: what the trainer plays is what the literature describes and what
// the Learner plugins do. Each block here pins one of those claims to a
// measurement, so a later "simplification" that quietly undoes it fails.
class SharedEnginesTest : public juce::UnitTest
{
public:
    SharedEnginesTest() : juce::UnitTest ("Shared engines (ADR 040)", "DSP") {}

    // |H| in dB of {b0 b1 b2 a0 a1 a2} at `hz`.
    static double digitalDb (const std::array<float, 6>& c, double hz, double fs)
    {
        const auto w = juce::MathConstants<double>::twoPi * hz / fs;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
        const auto num = (double) c[0] + (double) c[1] * z1 + (double) c[2] * z2;
        const auto den = (double) c[3] + (double) c[4] * z1 + (double) c[5] * z2;
        return 20.0 * std::log10 (std::abs (num / den));
    }

    // The analog bell the digital one claims to be.
    static double analogDb (double hz, double f0, double q, double gainDb)
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const std::complex<double> s (0.0, hz / f0);
        return 20.0 * std::log10 (std::abs ((s * s + s * (A / q) + 1.0) / (s * s + s / (A * q) + 1.0)));
    }

    void runTest() override
    {
        beginTest ("the matched bell keeps its shape up to 0.45 fs, where RBJ collapses");
        {
            auto worst = 0.0;

            for (const auto fs : { 44100.0, 48000.0, 96000.0 })
                for (const auto f0 : { 100.0, 1000.0, 5000.0, 10000.0, 16000.0 })
                    for (const auto gain : { 9.0f, 2.5f, -2.5f, -9.0f })
                    {
                        if (f0 > 0.45 * fs)
                            continue;

                        const auto c = EQCoefficients::makeMatchedBell (fs, (float) f0, 2.0f, gain);

                        // Centre exact, half an octave either side close.
                        expectWithinAbsoluteError (digitalDb (c, f0, fs), (double) gain, 0.05);

                        for (const auto ratio : { 0.7071, 1.4142 })
                        {
                            const auto hz = juce::jmin (f0 * ratio, fs * 0.499);
                            worst = juce::jmax (worst, std::abs (digitalDb (c, hz, fs) - analogDb (hz, f0, 2.0, gain)));
                        }
                    }

            // The number that motivated this: RBJ is 2.2 dB out half an
            // octave from a 16 kHz bell at 44.1 kHz.
            expectLessThan (worst, 1.6);
        }

        beginTest ("a matched cut is the exact reciprocal of the matched boost, and stable");
        {
            for (const auto f0 : { 60.0f, 2000.0f, 18000.0f })
            {
                const auto boost = EQCoefficients::makeMatchedBell (44100.0, f0, 1.4f, 7.0f);
                const auto cut = EQCoefficients::makeMatchedBell (44100.0, f0, 1.4f, -7.0f);

                for (const auto hz : { 30.0, 500.0, 5000.0, 15000.0, 21000.0 })
                    expectWithinAbsoluteError (digitalDb (boost, hz, 44100.0) + digitalDb (cut, hz, 44100.0), 0.0, 1.0e-3);

                // Poles of the cut inside the unit circle: |a2/a0| < 1 and
                // |a1| < a0 + a2 (the stability triangle).
                const auto a0 = (double) cut[3], a1 = (double) cut[4] / a0, a2 = (double) cut[5] / a0;
                expect (std::abs (a2) < 1.0 && std::abs (a1) < 1.0 + a2, "unstable cut at " + juce::String (f0));
            }
        }

        beginTest ("0 dB is a wire");
        {
            const auto c = EQCoefficients::makeMatchedBell (48000.0, 1000.0f, 1.0f, 0.0f);
            expectWithinAbsoluteError (digitalDb (c, 3000.0, 48000.0), 0.0, 1.0e-6);
        }

        beginTest ("the distortion antiderivatives differentiate back to the curves");
        {
            using T = DistortionGame::Type;

            for (const auto type : { T::softClip, T::hardClip, T::tapeSaturation, T::overdrive })
                for (const auto k : { 1.0f, 0.6f })
                    for (auto x = -4.0; x <= 4.0; x += 0.37)
                    {
                        constexpr double h = 1.0e-4;
                        const auto derivative = (DistortionGame::antiderivative (type, x + h, k)
                                                 - DistortionGame::antiderivative (type, x - h, k)) / (2.0 * h);
                        expectWithinAbsoluteError (derivative, (double) DistortionGame::shape (type, (float) x, k), 1.0e-3);
                    }
        }

        beginTest ("anti-aliasing takes the folded partials out of a hard clip");
        {
            // A 5 kHz sine hard-clipped at 44.1 kHz: its odd harmonics above
            // Nyquist fold back onto frequencies that are not multiples of
            // 5 kHz. Measure the energy that lands off the harmonic series.
            constexpr double fs = 44100.0, f = 4987.0;
            constexpr int n = 8192;

            const auto inharmonicShare = [&] (bool antiAliased)
            {
                juce::dsp::FFT fft (13);
                std::vector<float> data ((size_t) n * 2, 0.0f);
                auto previous = 0.0f;

                for (int i = 0; i < n; ++i)
                {
                    const auto x = 4.0f * (float) std::sin (juce::MathConstants<double>::twoPi * f * i / fs);
                    const auto y = antiAliased ? DistortionGame::shapeAntiAliased (DistortionGame::Type::hardClip, x, previous, 1.0f)
                                               : DistortionGame::shape (DistortionGame::Type::hardClip, x, 1.0f);
                    previous = x;
                    const auto window = 0.5f - 0.5f * (float) std::cos (juce::MathConstants<double>::twoPi * i / (n - 1));
                    data[(size_t) i] = y * window;
                }

                fft.performFrequencyOnlyForwardTransform (data.data());

                auto harmonic = 0.0, other = 0.0;
                for (int bin = 2; bin < n / 2; ++bin)
                {
                    const auto hz = bin * fs / n;
                    const auto nearest = std::round (hz / f) * f;
                    const auto e = (double) data[(size_t) bin] * data[(size_t) bin];
                    (std::abs (hz - nearest) < 60.0 && nearest > 0.0 ? harmonic : other) += e;
                }

                return other / (harmonic + other);
            };

            const auto naive = inharmonicShare (false);
            const auto adaa = inharmonicShare (true);

            logMessage ("inharmonic energy share: naive " + juce::String (naive, 4) + ", ADAA " + juce::String (adaa, 4));
            expectLessThan (adaa, naive * 0.5);
        }

        beginTest ("the exercise's own sound is levelled to the pink noise it replaces");
        {
            TestSignalGenerator generator;
            generator.setExerciseBed (LessonAudioBed::Bed::drumLoop, 44100.0, 2);

            expect (generator.hasBed());
            expect (! generator.isPlayingNoise());

            const auto rmsOf = [] (TestSignalGenerator& g, int count)
            {
                auto sum = 0.0;
                for (int i = 0; i < count; ++i)
                {
                    const auto v = (double) g.nextSample();
                    sum += v * v;
                }
                return std::sqrt (sum / count);
            };

            const auto bedRms = rmsOf (generator, (int) (4.8 * 44100.0));

            TestSignalGenerator noiseOnly;
            const auto noiseRms = rmsOf (noiseOnly, 1 << 18);

            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (bedRms / noiseRms), 0.0, 0.5);

            // Asking for pink noise gets pink noise.
            generator.setPreferBed (false);
            expect (generator.isPlayingNoise());
        }

        beginTest ("boosts first: no cuts on the first three steps");
        {
            expectEquals (Game::cutChanceForLevel (1), 0.0f);
            expectEquals (Game::cutChanceForLevel (3), 0.0f);
            expect (Game::cutChanceForLevel (10) > 0.4f);

            EQGame game;
            game.setDifficulty (2);
            game.prepare ({ 44100.0, 512, 1 });

            for (int round = 0; round < 200; ++round)
            {
                game.newRound();
                expectEquals (game.getAnswerDirection(), 1);
            }
        }

        beginTest ("a weak range is asked about more often");
        {
            // Everything weighted 1 except Low-mids at 6: Low-mids should
            // come up clearly more often than its share of octaves.
            EQGame game;
            game.prepare ({ 44100.0, 512, 1 });

            const auto lowMids = 2;
            std::vector<float> weights ((size_t) FrequencyRangeGame::numRanges, 1.0f);

            const auto shareOf = [&]
            {
                auto hits = 0;
                for (int round = 0; round < 1500; ++round)
                {
                    game.newRound();
                    game.submitNormalisedAnswer (0.0f);
                    hits += game.getSkillBucketForRound() == lowMids ? 1 : 0;
                }
                return (float) hits / 1500.0f;
            };

            const auto uniform = shareOf();
            weights[(size_t) lowMids] = 6.0f;
            game.setBucketWeights (weights);
            const auto weighted = shareOf();

            logMessage ("Low-mids share: uniform " + juce::String (uniform, 3) + ", weighted " + juce::String (weighted, 3));
            expect (weighted > uniform * 2.5f);

            // A value exercise: the same through keepDraw.
            PanGame pan;
            pan.prepare ({ 44100.0, 512, 2 });
            pan.setBucketWeights ({ 8.0f, 1.0f, 1.0f, 1.0f, 1.0f });

            auto hardLeft = 0;
            for (int round = 0; round < 1500; ++round)
            {
                pan.newRound();
                pan.submitNormalisedAnswer (0.5f);
                hardLeft += pan.getSkillBucketForRound() == 0 ? 1 : 0;
            }

            expect ((float) hardLeft / 1500.0f > 0.4f);
        }
    }
};

static SharedEnginesTest sharedEnginesTest;

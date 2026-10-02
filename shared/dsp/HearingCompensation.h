#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "shared/dsp/EQCoefficients.h"
#include <array>
#include <atomic>
#include <cmath>

// The Studio's hearing compensation (ADR 051): a set of bells per channel,
// at the audiometry test's frequencies, with the boost the profile asks
// for - the left ear's set on channel 0, the right ear's on channel 1.
//
// The bells are EQCoefficients::makeMatchedBell, the same bell Learner EQ
// runs, so the curve Learner EQ draws for it is exactly what the Studio
// does. Q 1.4 is about an octave wide: the test frequencies are an octave
// apart below 4 kHz and closer above, so neighbouring bells overlap into
// one smooth shape rather than a comb of peaks with dips between them.
//
// Knows nothing of profiles: shared/audio/HearingProfile turns a profile
// into Bands, the processor applies them.
namespace HearingCompensation
{
    inline constexpr int maxBands = 8;
    inline constexpr float bellQ = 1.4f;

    struct Bands
    {
        std::array<float, maxBands> freqHz {}, gainDb {};
        int count = 0;

        bool isFlat() const noexcept
        {
            for (int i = 0; i < count; ++i)
                if (std::abs (gainDb[(size_t) i]) > 0.01f)
                    return false;

            return true;
        }
    };

    template <typename FreqArray, typename GainArray>
    Bands makeBands (const FreqArray& frequencies, const GainArray& gains)
    {
        Bands b;
        b.count = juce::jmin (maxBands, (int) frequencies.size(), (int) gains.size());

        for (int i = 0; i < b.count; ++i)
        {
            b.freqHz[(size_t) i] = (float) frequencies[(size_t) i];
            b.gainDb[(size_t) i] = (float) gains[(size_t) i];
        }

        return b;
    }

    // One channel's filters, normalised so a0 = 1. Bands with no gain are
    // left out: a profile usually boosts two or three frequencies of eight.
    struct ChannelDesign
    {
        std::array<std::array<float, 5>, maxBands> c {};   // b0 b1 b2 a1 a2
        int count = 0;
    };

    inline ChannelDesign design (const Bands& bands, double sampleRate) noexcept
    {
        ChannelDesign d;

        for (int i = 0; i < bands.count; ++i)
        {
            const auto gain = bands.gainDb[(size_t) i];

            if (std::abs (gain) < 0.01f || bands.freqHz[(size_t) i] >= (float) (sampleRate * 0.49))
                continue;

            const auto raw = EQCoefficients::makeMatchedBell (sampleRate, bands.freqHz[(size_t) i], bellQ, gain);
            const auto a0 = std::abs (raw[3]) > 1.0e-12f ? raw[3] : 1.0f;
            d.c[(size_t) d.count++] = { raw[0] / a0, raw[1] / a0, raw[2] / a0, raw[4] / a0, raw[5] / a0 };
        }

        return d;
    }

    inline double responseDb (const ChannelDesign& d, double freqHz, double sampleRate) noexcept
    {
        double total = 0.0;

        for (int i = 0; i < d.count; ++i)
        {
            const auto& c = d.c[(size_t) i];
            const std::array<float, 6> full { c[0], c[1], c[2], 1.0f, c[3], c[4] };
            total += juce::Decibels::gainToDecibels (EQCoefficients::magnitudeOf (full, freqHz, sampleRate), -200.0);
        }

        return total;
    }

    // Bells an octave wide at frequencies closer than an octave add up: a
    // 4.5 dB bell at 8 kHz next to a 2.5 dB one at 6 kHz lands near 6 dB
    // at 8 kHz, not 4.5. So the gains asked for are targets for the
    // *combined* curve at the test frequencies, and the bells' own gains
    // are solved for.
    //
    // In dB, bells in series add, and each bell's shape scales almost
    // linearly with its gain - so "combined response at f_j = sum of
    // shape_i(f_j) x g_i" is a linear system, solved directly (eight
    // unknowns at most) and re-linearised a few times because a matched
    // bell's shape does change a little with its gain. The compensation
    // never cuts: a band whose solution comes out negative is fixed at zero
    // and the rest solved again. Where that happens - a small target right
    // beside a large one - the small one is overshot by its neighbour's
    // skirt rather than met with a cut. Only bands with a target take part:
    // the frequencies asked for nothing stay at nothing.
    inline Bands solveForTargets (const Bands& targets, double sampleRate) noexcept
    {
        auto bands = targets;
        std::array<bool, maxBands> free {};

        for (int i = 0; i < bands.count; ++i)
            free[(size_t) i] = targets.gainDb[(size_t) i] > 0.01f;

        // dB at f_j per dB of band i's gain, measured at band i's current
        // gain (or at 1 dB while it has none).
        const auto shape = [&] (int i, int j)
        {
            Bands one;
            one.count = 1;
            one.freqHz[0] = bands.freqHz[(size_t) i];
            const auto g = juce::jmax (1.0f, bands.gainDb[(size_t) i]);
            one.gainDb[0] = g;
            return responseDb (design (one, sampleRate), (double) bands.freqHz[(size_t) j], sampleRate) / (double) g;
        };

        for (int round = 0; round < 8; ++round)
        {
            std::array<int, maxBands> index {};
            int n = 0;

            for (int i = 0; i < bands.count; ++i)
                if (free[(size_t) i])
                    index[(size_t) n++] = i;

            if (n == 0)
                break;

            // S g = t over the free bands, by Gaussian elimination with
            // partial pivoting (S is diagonally dominant in practice).
            std::array<std::array<double, maxBands + 1>, maxBands> m {};

            for (int r = 0; r < n; ++r)
            {
                for (int c = 0; c < n; ++c)
                    m[(size_t) r][(size_t) c] = shape (index[(size_t) c], index[(size_t) r]);

                m[(size_t) r][(size_t) n] = (double) targets.gainDb[(size_t) index[(size_t) r]];
            }

            for (int c = 0; c < n; ++c)
            {
                auto pivot = c;

                for (int r = c + 1; r < n; ++r)
                    if (std::abs (m[(size_t) r][(size_t) c]) > std::abs (m[(size_t) pivot][(size_t) c]))
                        pivot = r;

                std::swap (m[(size_t) c], m[(size_t) pivot]);

                if (std::abs (m[(size_t) c][(size_t) c]) < 1.0e-9)
                    return targets;   // degenerate: fall back to the plain targets

                for (int r = 0; r < n; ++r)
                {
                    if (r == c)
                        continue;

                    const auto k = m[(size_t) r][(size_t) c] / m[(size_t) c][(size_t) c];

                    for (int col = c; col <= n; ++col)
                        m[(size_t) r][(size_t) col] -= k * m[(size_t) c][(size_t) col];
                }
            }

            for (int r = 0; r < n; ++r)
            {
                const auto g = (float) (m[(size_t) r][(size_t) n] / m[(size_t) r][(size_t) r]);
                const auto i = index[(size_t) r];

                if (g < 0.0f)
                {
                    free[(size_t) i] = false;
                    bands.gainDb[(size_t) i] = 0.0f;
                }
                else
                {
                    bands.gainDb[(size_t) i] = juce::jmin (g, targets.gainDb[(size_t) i]);
                }
            }
        }

        return bands;
    }

    // What the processor runs and what the curves draw: the bells solved
    // so the combined response meets each target.
    inline ChannelDesign designForTargets (const Bands& targets, double sampleRate) noexcept
    {
        return design (solveForTargets (targets, sampleRate), sampleRate);
    }

    // The combined response at a frequency, in dB.
    inline double responseDb (const Bands& targets, double freqHz, double sampleRate) noexcept
    {
        return responseDb (designForTargets (targets, sampleRate), freqHz, sampleRate);
    }

    // Runs the compensation on the audio thread.
    //
    // Coefficients are designed on whichever thread changes them (the
    // message thread when a profile is chosen, the device thread in
    // prepare) and handed to the audio thread through a triple buffer: the
    // writer fills a slot of its own and swaps it into the middle with one
    // atomic exchange; the audio thread swaps the middle for its own slot
    // only when a fresh one is waiting. Nobody ever waits and nothing is
    // allocated in process() - ADR 038, tests/RealtimeSafetyTest.
    class Processor
    {
    public:
        Processor() = default;

        // Message thread (or any thread but the audio one).
        void setBands (const Bands& left, const Bands& right, bool shouldBeActive)
        {
            const juce::ScopedLock lock (writerLock);
            leftBands = left;
            rightBands = right;
            active = shouldBeActive && ! (left.isFlat() && right.isFlat());
            publish();
        }

        void prepare (double newSampleRate)
        {
            const juce::ScopedLock lock (writerLock);
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
            publish();
        }

        bool isActive() const noexcept
        {
            const juce::ScopedLock lock (writerLock);
            return active;
        }

        // Audio thread. Channel 0 gets the left ear's set and channel 1 the
        // right's; a mono output gets the left's.
        void process (juce::AudioBuffer<float>& buffer) noexcept
        {
            if ((middle.load (std::memory_order_acquire) & freshBit) != 0)
            {
                front = middle.exchange (front, std::memory_order_acq_rel) & indexMask;

                // Switching on (or between profiles that change which
                // bands exist) starts from clean filter memory.
                if (slots[(size_t) front].active != wasActive || slots[(size_t) front].generation != lastGeneration)
                    state = {};

                wasActive = slots[(size_t) front].active;
                lastGeneration = slots[(size_t) front].generation;
            }

            const auto& slot = slots[(size_t) front];

            if (! slot.active)
                return;

            const auto numSamples = buffer.getNumSamples();

            for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
            {
                const auto& d = slot.channels[(size_t) ch];
                auto* data = buffer.getWritePointer (ch);

                for (int b = 0; b < d.count; ++b)
                {
                    const auto& c = d.c[(size_t) b];
                    auto& z = state[(size_t) ch][(size_t) b];

                    for (int i = 0; i < numSamples; ++i)
                    {
                        const auto x = data[i];
                        const auto y = c[0] * x + z[0];
                        z[0] = c[1] * x - c[3] * y + z[1];
                        z[1] = c[2] * x - c[4] * y;
                        data[i] = y;
                    }
                }
            }
        }

    private:
        struct Slot
        {
            std::array<ChannelDesign, 2> channels {};
            bool active = false;
            int generation = 0;
        };

        static constexpr int freshBit = 4;
        static constexpr int indexMask = 3;

        void publish()
        {
            auto& slot = slots[(size_t) back];
            slot.channels[0] = designForTargets (leftBands, sampleRate);
            slot.channels[1] = designForTargets (rightBands, sampleRate);
            slot.active = active;
            slot.generation = ++generation;
            back = middle.exchange (back | freshBit, std::memory_order_acq_rel) & indexMask;
        }

        juce::CriticalSection writerLock;   // writers only; process() never takes it
        Bands leftBands, rightBands;
        bool active = false;
        double sampleRate = 44100.0;
        int generation = 0;

        std::array<Slot, 3> slots {};
        int back = 2;                       // writer's
        std::atomic<int> middle { 1 };
        int front = 0;                      // audio thread's

        // Audio thread only.
        std::array<std::array<std::array<float, 2>, maxBands>, 2> state {};
        bool wasActive = false;
        int lastGeneration = 0;
    };
}

#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <vector>

// What a clip actually has in it: how much energy in each third of an
// octave, and whether it is hits or held sound. Measured once, when a clip
// is loaded, so an exercise can ask only about what is there.
//
// The bug this exists for (Bogdan, 2026-10-19): a hi-hat loop came up in
// "Name the range" and the question was "Sub-bass or Low-mids?". A hi-hat
// has nothing below a few hundred hertz, so a bell boosted at 40 Hz on it
// changes nothing - the round is unanswerable, and the player is marked
// wrong for not hearing a sound that was never there. The frequency
// exercises drew their target blind, from the whole spectrum, whatever was
// playing.
//
// What is measured, and why this way:
//
// - **Energy per third-octave, relative to the clip's own loudest band.**
//   A bell lifts the energy that is already near its centre; whether you
//   hear it depends on how much is there compared with the rest of the
//   sound, not on an absolute level. Pink noise reads flat by
//   construction, which is the reference every exercise was tuned on.
// - **Smoothed over one octave** (the band and its two neighbours) before
//   deciding. The narrowest bell any exercise uses is Q 2.4, about two
//   thirds of an octave wide, so a single empty third-octave between two
//   full ones is still a hearable boost.
// - **A plain double-precision FFT written out here**, not juce::dsp::FFT.
//   A battle round is judged on the server (tools/RoundOracle, Linux x86)
//   and replayed in the app (macOS/iOS, ARM), and JUCE picks a different
//   FFT engine on each (vDSP vs. its fallback). Which ranges count as
//   present decides which frequency is drawn, so both sides must reach the
//   same verdict from the same file. One implementation, in double, run on
//   the clip at its *source* sample rate (before resampling to the device
//   rate - 44.1 vs 48 kHz must not change the answer), is as close to
//   bit-identical as C++ gets, and the threshold is wide enough that a
//   last-bit difference cannot flip a real clip.
// - **Hits vs held** is the onset rate and the crest of the 10 ms level
//   envelope - the physical part ADR 025 says measurements can honestly do.
//
// Pure: no Component, no files, no message loop - tests/SpectralContentTest
// drives it with synthesized signals whose answer is known by construction.
namespace SpectralContent
{
    constexpr int numBands = 31;

    // Third-octave centres from 20 Hz to 20 kHz (ISO 266 nominal).
    inline const std::array<float, numBands>& bandCentres()
    {
        static const std::array<float, numBands> centres { {
            20.0f, 25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f, 125.0f, 160.0f,
            200.0f, 250.0f, 315.0f, 400.0f, 500.0f, 630.0f, 800.0f, 1000.0f, 1250.0f, 1600.0f,
            2000.0f, 2500.0f, 3150.0f, 4000.0f, 5000.0f, 6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f,
            20000.0f
        } };
        return centres;
    }

    // A band is "present" when its octave-smoothed energy is within this
    // many dB of the clip's loudest band. 30 dB is generous: a 2.5 dB bell
    // on a band 30 dB down is already a hard question; one 40 dB down (a
    // hi-hat's sub-bass, a kick's air) is no question at all.
    constexpr float presentWithinDb = 30.0f;

    // Below this the clip is silence, whatever its shape.
    constexpr float silenceFloorDb = -90.0f;

    struct Profile
    {
        bool analysed = false;

        // Octave-smoothed energy per band, dB relative to the loudest band
        // (so the loudest is 0 and everything else negative).
        std::array<float, numBands> relativeDb {};

        // Level of the clip, dBFS RMS.
        float rmsDb = -200.0f;

        // Onsets per second and the crest of the 10 ms level envelope (dB
        // between its loud frames and its median). A drum loop: several
        // onsets a second, crest well over 10 dB. A pad: crest of a few dB.
        float onsetsPerSecond = 0.0f;
        float envelopeCrestDb = 0.0f;

        bool isPresent (int band) const noexcept
        {
            if (! analysed)
                return true;   // nothing measured: no reason to exclude anything

            if (band < 0 || band >= numBands || rmsDb < silenceFloorDb)
                return false;

            return relativeDb[(size_t) band] >= -presentWithinDb;
        }

        // The third-octave a frequency falls in (nearest centre on a log
        // axis), and whether that one is present.
        static int bandIndexFor (float hz) noexcept
        {
            if (hz <= 0.0f)
                return 0;

            const auto position = 3.0f * std::log2 (hz / 1000.0f) + 17.0f;   // 1 kHz is band 17
            return juce::jlimit (0, numBands - 1, (int) std::lround (position));
        }

        bool isPresentAt (float hz) const noexcept { return isPresent (bandIndexFor (hz)); }

        // Whether any present band's centre lies in [lowHz, highHz).
        bool hasEnergyBetween (float lowHz, float highHz) const noexcept
        {
            if (! analysed)
                return true;

            for (int b = 0; b < numBands; ++b)
            {
                const auto c = bandCentres()[(size_t) b];
                if (c >= lowHz && c < highHz && isPresent (b))
                    return true;
            }

            return false;
        }

        // How many octaves of the spectrum are present - a sine is about
        // one (the smoothing spreads it), a hi-hat five or six, a mix ten.
        float presentOctaves() const noexcept
        {
            if (! analysed)
                return 10.0f;

            auto count = 0;
            for (int b = 0; b < numBands; ++b)
                if (isPresent (b))
                    ++count;

            return (float) count / 3.0f;
        }

        // Hits with space between them: at least one onset, and loud frames
        // more than 9 dB over the median one. 9.1 dB is the same line as
        // ReferenceAudioLibrary::sustainOf's 0.35 (median/loud), which the
        // delay, reverb and compression exercises already gate on - two
        // detectors that disagreed about the same clip would be a bug.
        static constexpr float hitsCrestDb = 9.1f;

        bool isHits() const noexcept
        {
            return analysed && onsetsPerSecond > 0.0f && envelopeCrestDb >= hitsCrestDb;
        }
    };

    namespace detail
    {
        // Iterative radix-2 FFT, in place; size is a power of two. The
        // twiddles come from a table built once per size from cos/sin of
        // exact angles - not by repeated multiplication, whose error would
        // accumulate differently on different machines.
        inline void fft (std::vector<std::complex<double>>& a)
        {
            const auto n = a.size();

            static std::vector<std::complex<double>> table;
            if (table.size() != n / 2)
            {
                table.resize (n / 2);
                for (size_t k = 0; k < n / 2; ++k)
                {
                    const auto angle = -2.0 * juce::MathConstants<double>::pi * (double) k / (double) n;
                    table[k] = { std::cos (angle), std::sin (angle) };
                }
            }

            for (size_t i = 1, j = 0; i < n; ++i)
            {
                auto bit = n >> 1;
                for (; (j & bit) != 0; bit >>= 1)
                    j ^= bit;
                j ^= bit;
                if (i < j)
                    std::swap (a[i], a[j]);
            }

            for (size_t len = 2; len <= n; len <<= 1)
            {
                const auto half = len / 2;
                const auto stride = n / len;

                for (size_t i = 0; i < n; i += len)
                {
                    for (size_t k = 0; k < half; ++k)
                    {
                        const auto u = a[i + k];
                        const auto v = a[i + k + half] * table[k * stride];
                        a[i + k] = u + v;
                        a[i + k + half] = u - v;
                    }
                }
            }
        }
    }

    // Mono samples at their own sample rate. Message thread: allocates.
    inline Profile analyse (const float* samples, int numSamples, double sampleRate)
    {
        Profile profile;

        if (samples == nullptr || numSamples <= 0 || sampleRate <= 0.0)
            return profile;

        // ---- level and envelope ------------------------------------------
        auto sumSquares = 0.0;
        for (int i = 0; i < numSamples; ++i)
            sumSquares += (double) samples[i] * (double) samples[i];

        const auto rms = std::sqrt (sumSquares / (double) numSamples);
        profile.rmsDb = rms > 1.0e-10 ? (float) (20.0 * std::log10 (rms)) : -200.0f;

        const auto frame = juce::jmax (1, (int) (sampleRate * 0.01));
        std::vector<double> envelope;
        for (int start = 0; start + frame <= numSamples; start += frame)
        {
            auto s = 0.0;
            for (int i = start; i < start + frame; ++i)
                s += (double) samples[i] * (double) samples[i];
            envelope.push_back (std::sqrt (s / frame));
        }

        if (envelope.size() >= 10)
        {
            auto sorted = envelope;
            std::sort (sorted.begin(), sorted.end());
            const auto loud = sorted[sorted.size() * 95 / 100];
            const auto median = sorted[sorted.size() / 2];
            profile.envelopeCrestDb = loud <= 1.0e-9 ? 0.0f
                                    : median <= 1.0e-9 ? 60.0f
                                    : (float) (20.0 * std::log10 (loud / median));

            // An onset: the level more than doubles (+6 dB) over the quietest
            // of the previous 30 ms and is within 20 dB of the loud frames.
            // Then 50 ms of refractory time, so one hit's ragged attack is
            // one onset rather than three.
            //
            // Before the clip counts as silence: a one-shot starts on its
            // hit, and comparing only within the clip missed exactly the
            // onset that defines it (every starter-pack kick read as 0/s).
            auto onsets = 0;
            auto refractory = 0;
            for (size_t i = 0; i < envelope.size(); ++i)
            {
                if (refractory > 0)
                {
                    --refractory;
                    continue;
                }

                auto before = std::numeric_limits<double>::max();
                for (size_t back = 1; back <= 3; ++back)
                    before = std::min (before, i >= back ? envelope[i - back] : 0.0);

                if (envelope[i] > 2.0 * before + 1.0e-6 && envelope[i] > loud * 0.1)
                {
                    ++onsets;
                    refractory = 5;
                }
            }

            profile.onsetsPerSecond = (float) ((double) onsets / ((double) numSamples / sampleRate));
        }

        // ---- spectrum ------------------------------------------------------
        // 8192 points: 5.4 Hz bins at 44.1 kHz, so even the 20 Hz third has
        // a bin of its own. Hann window, half overlap, power averaged.
        constexpr int fftSize = 8192;
        std::vector<double> power ((size_t) fftSize / 2 + 1, 0.0);
        std::vector<std::complex<double>> buffer ((size_t) fftSize);
        std::vector<double> window ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (fftSize - 1));

        // A clip shorter than one frame is zero-padded rather than ignored.
        // At most 32 frames - about 3 s at 44.1 kHz - since a loop repeats
        // and its first seconds already say what instrument is playing; it
        // runs on the message thread every time a round changes the clip.
        auto frames = 0;
        for (int start = 0; frames == 0 || (start + fftSize <= numSamples && frames < 32); start += fftSize / 2)
        {
            for (int i = 0; i < fftSize; ++i)
            {
                const auto index = start + i;
                const auto value = index < numSamples ? (double) samples[index] : 0.0;
                buffer[(size_t) i] = { value * window[(size_t) i], 0.0 };
            }

            detail::fft (buffer);

            for (size_t k = 0; k < power.size(); ++k)
                power[k] += std::norm (buffer[k]);

            ++frames;
        }

        const auto binHz = sampleRate / fftSize;
        const auto nyquist = sampleRate * 0.5;
        const auto edge = std::pow (2.0, 1.0 / 6.0);
        std::array<double, numBands> energy {};
        std::array<bool, numBands> measurable {};

        for (int b = 0; b < numBands; ++b)
        {
            const auto centre = (double) bandCentres()[(size_t) b];
            const auto low = centre / edge;
            const auto high = juce::jmin (centre * edge, nyquist);
            measurable[(size_t) b] = low < nyquist;

            auto sum = 0.0;
            auto bins = 0;
            for (size_t k = 1; k < power.size(); ++k)
            {
                const auto hz = (double) k * binHz;
                if (hz >= low && hz < high)
                {
                    sum += power[k];
                    ++bins;
                }
            }

            // Narrower than one bin (only the lowest bands at very low
            // sample rates): the nearest bin, scaled to the band's width.
            if (bins == 0 && measurable[(size_t) b])
            {
                const auto k = (size_t) juce::jlimit (1, (int) power.size() - 1, (int) std::round (centre / binHz));
                sum = power[k] * ((high - low) / binHz);
            }

            energy[(size_t) b] = sum;
        }

        // Octave smoothing: a band and its two neighbours.
        std::array<double, numBands> smoothed {};
        for (int b = 0; b < numBands; ++b)
        {
            if (! measurable[(size_t) b])
                continue;

            auto s = 0.0;
            auto n = 0;
            for (int j = juce::jmax (0, b - 1); j <= juce::jmin (numBands - 1, b + 1); ++j)
            {
                if (measurable[(size_t) j])
                {
                    s += energy[(size_t) j];
                    ++n;
                }
            }
            smoothed[(size_t) b] = n > 0 ? s / n : 0.0;
        }

        const auto peak = *std::max_element (smoothed.begin(), smoothed.end());

        for (int b = 0; b < numBands; ++b)
        {
            const auto v = smoothed[(size_t) b];
            profile.relativeDb[(size_t) b] = (peak > 0.0 && v > 0.0)
                                                ? (float) (10.0 * std::log10 (v / peak))
                                                : -200.0f;
        }

        profile.analysed = true;
        return profile;
    }
}

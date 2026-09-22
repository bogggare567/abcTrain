#include "shared/audio/StemSeparator.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace StemSeparator
{
    namespace
    {
        using Complex = std::complex<float>;

        // Smoothstep on [low, high]: 0 below, 1 above, no corners.
        float smoothStep (float low, float high, float x) noexcept
        {
            if (high <= low)
                return x >= high ? 1.0f : 0.0f;

            const auto t = juce::jlimit (0.0f, 1.0f, (x - low) / (high - low));
            return t * t * (3.0f - 2.0f * t);
        }

        // A sorted run of values that a window slides over: one value in,
        // one out, each a binary search and a short move. Both medians use
        // it, which is what keeps a whole song to seconds - re-selecting
        // the median of every window from scratch was most of the cost.
        void sortedInsert (float* sorted, int& count, float value) noexcept
        {
            auto* position = std::upper_bound (sorted, sorted + count, value);
            std::move_backward (position, sorted + count, sorted + count + 1);
            *position = value;
            ++count;
        }

        void sortedRemove (float* sorted, int& count, float value) noexcept
        {
            if (count <= 0)
                return;

            // The value is present bit-for-bit (it was inserted from the
            // same float), so the first element not below it is it.
            auto* position = std::lower_bound (sorted, sorted + count, value);
            if (position == sorted + count)
                --position;

            std::move (position + 1, sorted + count, position);
            --count;
        }

        int oddAtLeastThree (double value) noexcept
        {
            auto n = juce::jmax (3, (int) std::lround (value));
            return (n % 2 == 0) ? n + 1 : n;
        }
    }

    Result separate (const juce::AudioBuffer<float>& audio,
                     double sampleRate,
                     const Options& options,
                     std::function<void (float)> onProgress,
                     std::function<bool()> shouldStop)
    {
        Result result;

        const auto numSamples = audio.getNumSamples();
        const auto numInputChannels = audio.getNumChannels();

        if (numSamples <= 0 || numInputChannels <= 0 || ! (sampleRate > 0.0))
        {
            result.completed = true;
            return result;
        }

        const auto stereo = numInputChannels >= 2;
        const auto numChannels = stereo ? 2 : 1;

        for (auto& stem : result.stems)
        {
            stem.setSize (numChannels, numSamples);
            stem.clear();
        }

        // ~93 ms window whatever the rate: 4096 at 44.1/48 kHz, 8192 at
        // 88.2/96 kHz. Long enough to resolve a bass note's harmonics,
        // short enough that a drum hit stays a thin vertical line.
        const auto fftOrder = juce::jlimit (10, 14, (int) std::lround (std::log2 (sampleRate * 0.0929)));
        const auto fftSize = 1 << fftOrder;
        const auto hop = fftSize / 4;
        const auto numBins = fftSize / 2 + 1;
        const auto binHz = sampleRate / (double) fftSize;

        // Periodic Hann for analysis and synthesis. At a hop of a quarter
        // window, the overlapping squared windows sum to exactly 1.5
        // everywhere, so dividing by that is the whole normalisation - as
        // long as every sample is covered by all four frames, which is why
        // the first frame starts (fftSize - hop) samples before the audio.
        std::vector<float> window ((size_t) fftSize);
        for (int n = 0; n < fftSize; ++n)
            window[(size_t) n] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) fftSize);

        const auto synthesisScale = 1.0f / 1.5f;
        const auto firstFrameOffset = -(fftSize - hop);
        const auto numFrames = (numSamples + fftSize - hop + hop - 1) / hop;

        const auto timeLength = oddAtLeastThree (options.harmonicMedianSeconds * sampleRate / (double) hop);
        const auto timeHalf = timeLength / 2;
        const auto freqLength = oddAtLeastThree (options.percussiveMedianHz / binHz);
        const auto freqHalf = freqLength / 2;

        // Bass crossover: eighth-order low-pass shape, 0.5 at the crossover.
        std::vector<float> bassWeight ((size_t) numBins);
        for (int k = 0; k < numBins; ++k)
        {
            const auto ratio = (double) k * binHz / juce::jmax (1.0, options.bassCrossoverHz);
            bassWeight[(size_t) k] = (float) (1.0 / (1.0 + std::pow (ratio, 8.0)));
        }

        // The ring: only the frames the time median can see. Each frame's
        // spectrum is kept packed - left in the real part, right in the
        // imaginary - which works because every mask here is real and the
        // same for both channels: masking the packed spectrum masks both
        // channels at once, and one inverse FFT gives both back.
        const auto ringSize = timeLength;
        std::vector<Complex> ringPacked ((size_t) (ringSize * fftSize));
        std::vector<float> ringMagnitude ((size_t) (ringSize * numBins));
        std::vector<float> ringPowerL ((size_t) (ringSize * numBins));
        std::vector<float> ringPowerR ((size_t) (ringSize * numBins));
        std::vector<Complex> ringCross ((size_t) (ringSize * numBins));

        juce::dsp::FFT fft (fftOrder);
        std::vector<Complex> timeDomain ((size_t) fftSize);
        std::vector<Complex> spectrum ((size_t) fftSize);

        std::array<std::vector<float>, numStems> masks;
        for (auto& m : masks)
            m.resize ((size_t) numBins);

        // Running sums of the cross- and auto-spectra over the frames in
        // the ring, so the coherence of each frame's window costs one
        // lookup per bin rather than a loop over the window. Double, since
        // they are added to and subtracted from for the whole song.
        std::vector<std::complex<double>> crossSum ((size_t) numBins);
        std::vector<double> powerSum ((size_t) numBins);

        auto accumulate = [&] (int frame, double sign)
        {
            const auto offset = (size_t) ((frame % ringSize) * numBins);

            for (int k = 0; k < numBins; ++k)
            {
                const auto c = ringCross[offset + (size_t) k];
                crossSum[(size_t) k] += sign * std::complex<double> (c.real(), c.imag());
                powerSum[(size_t) k] += sign * ((double) ringPowerL[offset + (size_t) k] + (double) ringPowerR[offset + (size_t) k]);
            }
        };

        // Per bin, the magnitudes of the frames in the ring, kept sorted -
        // the time median is then just the middle element.
        std::vector<float> sortedAcrossTime ((size_t) (numBins * ringSize));
        auto framesInWindow = 0;

        std::vector<float> sortedAcrossFrequency ((size_t) freqLength + 1);

        const auto* inL = audio.getReadPointer (0);
        const auto* inR = stereo ? audio.getReadPointer (1) : nullptr;

        const auto maskPower = juce::jmax (0.5f, options.maskPower);
        const auto squareMasks = std::abs (maskPower - 2.0f) < 1.0e-6f;   // the default, and much cheaper than pow
        constexpr float tiny = 1.0e-20f;

        auto analyseFrame = [&] (int frame)
        {
            const auto start = frame * hop + firstFrameOffset;
            const auto slot = frame % ringSize;

            for (int n = 0; n < fftSize; ++n)
            {
                const auto i = start + n;
                const auto inside = i >= 0 && i < numSamples;
                const auto l = inside ? inL[i] : 0.0f;
                const auto r = (inside && inR != nullptr) ? inR[i] : 0.0f;
                timeDomain[(size_t) n] = Complex (l * window[(size_t) n], r * window[(size_t) n]);
            }

            auto* packed = ringPacked.data() + (size_t) (slot * fftSize);
            fft.perform (reinterpret_cast<const juce::dsp::Complex<float>*> (timeDomain.data()),
                         reinterpret_cast<juce::dsp::Complex<float>*> (packed), false);

            auto* magnitude = ringMagnitude.data() + (size_t) (slot * numBins);
            auto* powerL = ringPowerL.data() + (size_t) (slot * numBins);
            auto* powerR = ringPowerR.data() + (size_t) (slot * numBins);
            auto* cross = ringCross.data() + (size_t) (slot * numBins);

            for (int k = 0; k < numBins; ++k)
            {
                const auto z = packed[k];
                const auto zMirror = std::conj (packed[(fftSize - k) % fftSize]);

                // Unpack the two real channels' spectra from the packed one.
                const auto left  = stereo ? 0.5f * (z + zMirror) : z;
                const auto right = stereo ? Complex (0.0f, -0.5f) * (z - zMirror) : z;

                const auto pl = std::norm (left);
                const auto pr = std::norm (right);

                powerL[k] = pl;
                powerR[k] = pr;
                cross[k] = left * std::conj (right);

                // The masks are computed from one magnitude for both
                // channels, so the stereo image survives. Power-averaged
                // rather than a literal (L+R)/2 mid: anti-phase content
                // cancels out of a mid signal and would then be sorted by
                // nothing at all.
                magnitude[k] = std::sqrt (0.5f * (pl + pr));
            }
        };

        auto synthesiseFrame = [&] (int frame)
        {
            const auto slot = frame % ringSize;
            const auto* magnitude = ringMagnitude.data() + (size_t) (slot * numBins);

            auto binsInWindow = 0;

            for (int k = 0; k < juce::jmin (freqHalf, numBins); ++k)
                sortedInsert (sortedAcrossFrequency.data(), binsInWindow, magnitude[k]);

            for (int k = 0; k < numBins; ++k)
            {
                // Median across frequency - the percussive estimate.
                if (k + freqHalf < numBins)
                    sortedInsert (sortedAcrossFrequency.data(), binsInWindow, magnitude[k + freqHalf]);

                if (k - freqHalf - 1 >= 0)
                    sortedRemove (sortedAcrossFrequency.data(), binsInWindow, magnitude[k - freqHalf - 1]);

                const auto percussiveMedian = sortedAcrossFrequency[(size_t) (binsInWindow / 2)];

                // Median across time - the harmonic estimate.
                const auto hm = sortedAcrossTime[(size_t) (k * ringSize + framesInWindow / 2)];
                const auto hp = squareMasks ? hm * hm : std::pow (hm, maskPower);
                const auto pp = squareMasks ? percussiveMedian * percussiveMedian
                                            : std::pow (percussiveMedian, maskPower);
                const auto denominator = hp + pp;
                const auto percussive = denominator > tiny ? pp / denominator : 0.0f;
                const auto harmonic = 1.0f - percussive;

                const auto bass = harmonic * bassWeight[(size_t) k];
                const auto rest = harmonic - bass;

                auto centreShare = 1.0f;

                if (stereo)
                {
                    // Coherence over the same window the time median uses,
                    // centred on this frame. Averaging is the point: one
                    // frame's |L·R*| is just |L||R|, which only measures
                    // level balance and would call a wide, decorrelated pad
                    // with equal levels "centre".
                    //
                    // The real part rather than the magnitude of the
                    // cross-spectrum: content in anti-phase between the
                    // speakers is as wide as a mix gets, and |L·R*| would
                    // call it perfectly centred.
                    const auto power = powerSum[(size_t) k];
                    const auto similarity = power > 1.0e-12 * (double) fftSize
                                                ? juce::jmax (0.0f, (float) (2.0 * crossSum[(size_t) k].real() / power))
                                                : 1.0f;
                    centreShare = smoothStep (options.centreSimilarityLow, options.centreSimilarityHigh, similarity);
                }

                const auto centre = rest * centreShare;

                masks[(size_t) Stem::drums][(size_t) k]  = percussive;
                masks[(size_t) Stem::bass][(size_t) k]   = bass;
                masks[(size_t) Stem::centre][(size_t) k] = centre;
                masks[(size_t) Stem::sides][(size_t) k]  = rest - centre;
            }

            const auto* packed = ringPacked.data() + (size_t) (slot * fftSize);
            const auto start = frame * hop + firstFrameOffset;

            for (int s = 0; s < numStems; ++s)
            {
                const auto& mask = masks[(size_t) s];

                for (int k = 0; k < fftSize; ++k)
                    spectrum[(size_t) k] = packed[k] * mask[(size_t) juce::jmin (k, fftSize - k)];

                fft.perform (reinterpret_cast<const juce::dsp::Complex<float>*> (spectrum.data()),
                             reinterpret_cast<juce::dsp::Complex<float>*> (timeDomain.data()), true);

                auto* outL = result.stems[(size_t) s].getWritePointer (0);
                auto* outR = stereo ? result.stems[(size_t) s].getWritePointer (1) : nullptr;

                const auto nFirst = juce::jmax (0, -start);
                const auto nLast = juce::jmin (fftSize, numSamples - start);

                for (int n = nFirst; n < nLast; ++n)
                {
                    const auto gain = window[(size_t) n] * synthesisScale;
                    const auto value = timeDomain[(size_t) n];
                    outL[start + n] += value.real() * gain;

                    if (outR != nullptr)
                        outR[start + n] += value.imag() * gain;
                }
            }
        };

        // Streaming: frame f is analysed, and the frame timeHalf behind it -
        // whose whole time-median window is now in the ring - is separated
        // and written out.
        for (int f = 0; f < numFrames + timeHalf; ++f)
        {
            if ((f & 31) == 0 && shouldStop != nullptr && shouldStop())
            {
                for (auto& stem : result.stems)
                    stem.setSize (0, 0);

                result.completed = false;
                return result;
            }

            // Keep the running sums equal to exactly the frames in the
            // time window of the frame about to be separated: the one
            // falling out of the ring leaves before its slot is reused.
            const auto leaving = f - ringSize;

            if (leaving >= 0 && leaving < numFrames)
            {
                if (stereo)
                    accumulate (leaving, -1.0);

                const auto* old = ringMagnitude.data() + (size_t) ((leaving % ringSize) * numBins);
                auto count = 0;

                for (int k = 0; k < numBins; ++k)
                {
                    count = framesInWindow;
                    sortedRemove (sortedAcrossTime.data() + (size_t) (k * ringSize), count, old[k]);
                }

                framesInWindow = count;
            }

            if (f < numFrames)
            {
                analyseFrame (f);

                if (stereo)
                    accumulate (f, 1.0);

                const auto* fresh = ringMagnitude.data() + (size_t) ((f % ringSize) * numBins);
                auto count = 0;

                for (int k = 0; k < numBins; ++k)
                {
                    count = framesInWindow;
                    sortedInsert (sortedAcrossTime.data() + (size_t) (k * ringSize), count, fresh[k]);
                }

                framesInWindow = count;
            }

            const auto ready = f - timeHalf;

            if (ready >= 0)
                synthesiseFrame (ready);

            if ((f & 63) == 0 && onProgress != nullptr)
                onProgress ((float) f / (float) (numFrames + timeHalf));
        }

        if (onProgress != nullptr)
            onProgress (1.0f);

        result.completed = true;
        return result;
    }

    const char* folderNameFor (Stem stem) noexcept
    {
        switch (stem)
        {
            case Stem::drums:  return "Stem - Drums";
            case Stem::bass:   return "Stem - Bass";
            case Stem::centre: return "Stem - Centre (Vocals)";
            case Stem::sides:  return "Stem - Sides (Wide)";
        }

        return "Stem - Centre (Vocals)";
    }
}

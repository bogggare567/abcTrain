#include "AudioSliceAnalyzer.h"
#include <cmath>

namespace AudioSliceAnalyzer
{
    namespace
    {
        constexpr int fftOrder = 10;             // 1024 - ~23ms at 44.1k
        constexpr int fftSize = 1 << fftOrder;
        constexpr int hopSize = fftSize / 2;

        // Band edges, in Hz. Chosen to match the vocabulary the rest of the
        // app already uses (see docs/knowledge_base.md): bass below 250,
        // the range voices and leads occupy up to about 4k, air above.
        constexpr float bassTopHz = 250.0f;
        constexpr float midTopHz = 4000.0f;

        // Mono sum, because every measurement here is about *content* and
        // none of them is about the stereo field except the correlation,
        // which is computed separately from the real channels.
        std::vector<float> monoSum (const juce::AudioBuffer<float>& audio,
                                    int startSample, int numSamples)
        {
            std::vector<float> mono ((size_t) numSamples, 0.0f);
            const auto channels = juce::jmax (1, audio.getNumChannels());

            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            {
                const auto* source = audio.getReadPointer (channel);

                for (int i = 0; i < numSamples; ++i)
                    mono[(size_t) i] += source[startSample + i];
            }

            const auto scale = 1.0f / (float) channels;

            for (auto& sample : mono)
                sample *= scale;

            return mono;
        }

        float rmsOf (const std::vector<float>& samples)
        {
            if (samples.empty())
                return 0.0f;

            auto sum = 0.0;

            for (const auto sample : samples)
                sum += (double) sample * (double) sample;

            return (float) std::sqrt (sum / (double) samples.size());
        }

        // Pearson correlation between the two channels: +1 is mono, 0 is
        // fully decorrelated, negative means out of phase.
        float correlationOf (const juce::AudioBuffer<float>& audio, int startSample, int numSamples)
        {
            if (audio.getNumChannels() < 2 || numSamples <= 0)
                return 1.0f;

            const auto* left = audio.getReadPointer (0) + startSample;
            const auto* right = audio.getReadPointer (1) + startSample;

            auto sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;

            for (int i = 0; i < numSamples; ++i)
            {
                sumLR += (double) left[i] * (double) right[i];
                sumLL += (double) left[i] * (double) left[i];
                sumRR += (double) right[i] * (double) right[i];
            }

            const auto denominator = std::sqrt (sumLL * sumRR);

            // Silence is not "uncorrelated", it is "no information" - and
            // calling it 0 would sort every quiet clip as wide.
            return denominator > 1.0e-9 ? (float) (sumLR / denominator) : 1.0f;
        }

        struct SpectralSummary
        {
            float centroidHz = 0.0f;
            float onsetsPerSecond = 0.0f;
            float bassFraction = 0.0f;
            float midFraction = 0.0f;
            float highFraction = 0.0f;

            // How much of the passage is *loud*, as a fraction of its
            // frames. This is what separates a drum loop from a mix that
            // happens to contain drums, and running the analyser over real
            // music is what showed it was needed: onset density alone
            // called nearly every commercial track "percussive", because
            // any mix with a beat has several onsets a second. A drum loop
            // has silence between the hits; a finished mix does not.
            float sustainFraction = 0.0f;
        };

        SpectralSummary summarise (const std::vector<float>& mono, double sampleRate)
        {
            SpectralSummary summary;

            if ((int) mono.size() < fftSize)
                return summary;

            juce::dsp::FFT fft (fftOrder);
            juce::dsp::WindowingFunction<float> window ((size_t) fftSize,
                                                         juce::dsp::WindowingFunction<float>::hann);

            std::vector<float> scratch ((size_t) fftSize * 2, 0.0f);
            std::vector<float> previousMagnitudes ((size_t) fftSize / 2, 0.0f);

            auto weightedSum = 0.0;
            auto magnitudeSum = 0.0;
            auto bassEnergy = 0.0, midEnergy = 0.0, highEnergy = 0.0;

            // Spectral flux, summed per frame: the standard cheap onset
            // measure. Only *rises* count - energy falling away is a note
            // ending, not a note starting.
            std::vector<float> flux;
            auto frames = 0;

            for (int start = 0; start + fftSize <= (int) mono.size(); start += hopSize)
            {
                std::fill (scratch.begin(), scratch.end(), 0.0f);
                std::copy (mono.begin() + start, mono.begin() + start + fftSize, scratch.begin());

                window.multiplyWithWindowingTable (scratch.data(), (size_t) fftSize);
                fft.performFrequencyOnlyForwardTransform (scratch.data());

                auto frameFlux = 0.0f;

                for (int bin = 1; bin < fftSize / 2; ++bin)
                {
                    const auto magnitude = scratch[(size_t) bin];
                    const auto frequency = (float) bin * (float) sampleRate / (float) fftSize;

                    weightedSum += (double) magnitude * (double) frequency;
                    magnitudeSum += (double) magnitude;

                    if (frequency < bassTopHz)       bassEnergy += magnitude;
                    else if (frequency < midTopHz)   midEnergy += magnitude;
                    else                             highEnergy += magnitude;

                    frameFlux += juce::jmax (0.0f, magnitude - previousMagnitudes[(size_t) bin]);
                    previousMagnitudes[(size_t) bin] = magnitude;
                }

                flux.push_back (frameFlux);
                ++frames;
            }

            if (frames == 0 || magnitudeSum <= 0.0)
                return summary;

            summary.centroidHz = (float) (weightedSum / magnitudeSum);

            const auto totalEnergy = bassEnergy + midEnergy + highEnergy;

            if (totalEnergy > 0.0)
            {
                summary.bassFraction = (float) (bassEnergy / totalEnergy);
                summary.midFraction  = (float) (midEnergy / totalEnergy);
                summary.highFraction = (float) (highEnergy / totalEnergy);
            }

            // An onset is a flux frame well above the median *and* a real
            // fraction of the loudest rise in the passage.
            //
            // The median alone is not enough, and a test caught it: for a
            // sustained tone the flux is essentially zero everywhere, so a
            // median-only threshold collapses to nothing and floating-point
            // noise reads as a stream of onsets - a held note classified as
            // a drum loop. The peak-relative floor is what makes "loud
            // compared to what else happens here" mean something when
            // nothing much happens here.
            auto sorted = flux;
            std::sort (sorted.begin(), sorted.end());

            const auto median = sorted[sorted.size() / 2];
            const auto peak = sorted.back();
            const auto threshold = juce::jmax (median * 2.2f, peak * 0.15f);

            auto onsets = 0;
            auto armed = true;

            for (const auto value : flux)
            {
                if (armed && value > threshold)
                {
                    ++onsets;
                    armed = false;      // one onset per rise, not one per frame
                }
                else if (value < threshold * 0.6f)
                {
                    armed = true;
                }
            }

            const auto seconds = (double) mono.size() / sampleRate;
            summary.onsetsPerSecond = seconds > 0.0 ? (float) ((double) onsets / seconds) : 0.0f;

            // Duty cycle, measured over short frames against the passage's
            // own level - so it is about *shape*, not about how loud the
            // recording was mastered.
            {
                const auto frameSize = juce::jmax (64, (int) (sampleRate * 0.02));
                auto overall = 0.0;

                for (const auto sample : mono)
                    overall += (double) sample * (double) sample;

                overall = std::sqrt (overall / (double) mono.size());

                if (overall > 1.0e-9)
                {
                    auto loudFrames = 0, totalFrames = 0;

                    for (int start = 0; start + frameSize <= (int) mono.size(); start += frameSize)
                    {
                        auto energy = 0.0;

                        for (int i = 0; i < frameSize; ++i)
                            energy += (double) mono[(size_t) (start + i)] * (double) mono[(size_t) (start + i)];

                        if (std::sqrt (energy / (double) frameSize) > overall * 0.35)
                            ++loudFrames;

                        ++totalFrames;
                    }

                    if (totalFrames > 0)
                        summary.sustainFraction = (float) loudFrames / (float) totalFrames;
                }
            }

            return summary;
        }

        Character classify (const SpectralSummary& summary, float correlation)
        {
            juce::ignoreUnused (correlation);

            // Percussive means "this is mostly hits", not "this contains
            // hits". Two conditions, and the second one is the important
            // one: dense onsets *and* real gaps between them. Running this
            // over actual records is what taught it - with onset density
            // alone, every commercial mix came back percussive, because
            // every mix with a beat has several onsets a second. Adding
            // the duty-cycle test is what makes the label mean something.
            if (summary.onsetsPerSecond >= 2.5f && summary.sustainFraction < 0.78f)
                return Character::percussive;

            // A single band carrying most of the energy is the clearest
            // signal there is - a bass line, or cymbals and air.
            if (summary.bassFraction > 0.55f)
                return Character::bass;

            if (summary.highFraction > 0.4f)
                return Character::bright;

            // Energy in all three bands with none of them dominating is a
            // finished mix, whatever else is going on in it.
            const auto broad = summary.bassFraction > 0.15f
                                   && summary.midFraction > 0.25f
                                   && summary.highFraction > 0.10f;

            if (broad)
                return Character::fullRange;

            if (summary.midFraction > 0.45f)
                return Character::midRange;

            return Character::fullRange;
        }

        // Nudges a cut toward the quietest point nearby, so a loop starts
        // and ends in a gap rather than halfway through a note.
        int snapToQuietPoint (const juce::AudioBuffer<float>& audio, int idealStart,
                              int searchRadius, int windowSamples)
        {
            const auto lowest = juce::jmax (0, idealStart - searchRadius);
            const auto highest = juce::jmin (audio.getNumSamples() - windowSamples,
                                              idealStart + searchRadius);

            if (highest <= lowest)
                return juce::jlimit (0, juce::jmax (0, audio.getNumSamples() - windowSamples), idealStart);

            auto bestStart = idealStart;
            auto bestEnergy = std::numeric_limits<float>::max();

            // A short window either side of each candidate - the energy
            // *at* the cut is what clicks, not the energy of the whole clip.
            const auto probe = juce::jmax (64, windowSamples / 200);

            for (auto candidate = lowest; candidate <= highest; candidate += probe)
            {
                auto energy = 0.0f;

                for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                {
                    const auto* source = audio.getReadPointer (channel);

                    for (int i = 0; i < probe && candidate + i < audio.getNumSamples(); ++i)
                        energy += std::abs (source[candidate + i]);
                }

                if (energy < bestEnergy)
                {
                    bestEnergy = energy;
                    bestStart = candidate;
                }
            }

            return bestStart;
        }
    }

    std::vector<Slice> analyse (const juce::AudioBuffer<float>& audio,
                                double sampleRate,
                                const Options& options)
    {
        std::vector<Slice> slices;

        if (sampleRate <= 0.0 || audio.getNumSamples() <= 0 || audio.getNumChannels() <= 0)
            return slices;

        const auto sliceSamples = (int) (options.sliceSeconds * sampleRate);

        if (sliceSamples < fftSize || audio.getNumSamples() < sliceSamples)
            return slices;

        const auto searchRadius = (int) (options.snapWindow * (double) sliceSamples);

        for (auto position = 0; position + sliceSamples <= audio.getNumSamples(); position += sliceSamples)
        {
            const auto start = snapToQuietPoint (audio, position, searchRadius, sliceSamples);

            if (start + sliceSamples > audio.getNumSamples())
                break;

            const auto mono = monoSum (audio, start, sliceSamples);

            Slice slice;
            slice.startSample = start;
            slice.numSamples = sliceSamples;
            slice.rms = rmsOf (mono);

            if (slice.rms < options.minimumRms)
                continue;   // an intro, a fade or a gap - not training material

            const auto summary = summarise (mono, sampleRate);

            slice.onsetsPerSecond = summary.onsetsPerSecond;
            slice.spectralCentroidHz = summary.centroidHz;
            slice.stereoCorrelation = correlationOf (audio, start, sliceSamples);
            slice.character = classify (summary, slice.stereoCorrelation);

            slices.push_back (slice);
        }

        // Thin down to the cap, evenly. Taking the first N would give six
        // clips of the intro; this walks the whole track.
        if (options.maxSlicesPerFile > 0 && (int) slices.size() > options.maxSlicesPerFile)
        {
            std::vector<Slice> kept;
            kept.reserve ((size_t) options.maxSlicesPerFile);

            const auto step = (double) slices.size() / (double) options.maxSlicesPerFile;

            for (int i = 0; i < options.maxSlicesPerFile; ++i)
                kept.push_back (slices[(size_t) (i * step)]);

            return kept;
        }

        return slices;
    }

    const char* folderNameFor (Character character) noexcept
    {
        switch (character)
        {
            case Character::percussive: return "Percussive";
            case Character::bass:       return "Bass";
            case Character::midRange:   return "Mid Range";
            case Character::bright:     return "Bright";
            case Character::fullRange:  return "Full Mix";
        }

        return "Full Mix";
    }

    const char* nameKeyFor (Character character) noexcept
    {
        switch (character)
        {
            case Character::percussive: return "slice.percussive";
            case Character::bass:       return "slice.bass";
            case Character::midRange:   return "slice.midRange";
            case Character::bright:     return "slice.bright";
            case Character::fullRange:  return "slice.fullRange";
        }

        return "slice.fullRange";
    }
}

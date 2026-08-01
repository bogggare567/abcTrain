#include "AudioSliceAnalyzer.h"
#include <algorithm>
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

        // ---- tempo ------------------------------------------------------
        //
        // Three steps, each the cheap standard one:
        //
        //   1. an onset envelope - spectral flux per frame, rises only,
        //      because energy falling away is a note ending
        //   2. autocorrelation of that envelope over the plausible beat
        //      periods; music repeats at its beat, so the period that
        //      correlates best with itself is the beat
        //   3. the phase of the grid, by testing every offset within one
        //      beat and keeping the one where the pulses land on the most
        //      onset energy
        //
        // Deliberately not a beat tracker: no tempo changes, no downbeat
        // detection, no swing. Those need a model and would be wrong
        // confidently. This answers one question - "is there a steady pulse
        // here, and where is it" - and says so honestly when the answer is
        // no, which is the case for a pad, a spoken word recording or a
        // rubato piano piece.
        std::vector<float> onsetEnvelope (const juce::AudioBuffer<float>& audio, int& hopOut)
        {
            std::vector<float> envelope;
            hopOut = hopSize;

            const auto numSamples = audio.getNumSamples();

            if (numSamples < fftSize)
                return envelope;

            juce::dsp::FFT fft (fftOrder);
            juce::dsp::WindowingFunction<float> window ((size_t) fftSize,
                                                         juce::dsp::WindowingFunction<float>::hann);

            std::vector<float> scratch ((size_t) fftSize * 2, 0.0f);
            std::vector<float> previous ((size_t) fftSize / 2, 0.0f);

            for (int start = 0; start + fftSize <= numSamples; start += hopSize)
            {
                std::fill (scratch.begin(), scratch.end(), 0.0f);

                // Mono sum, in place, so this walks the file once rather
                // than building a copy of it - an album track is tens of
                // millions of samples.
                for (int i = 0; i < fftSize; ++i)
                {
                    auto sum = 0.0f;

                    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                        sum += audio.getReadPointer (channel)[start + i];

                    scratch[(size_t) i] = sum / (float) audio.getNumChannels();
                }

                window.multiplyWithWindowingTable (scratch.data(), (size_t) fftSize);
                fft.performFrequencyOnlyForwardTransform (scratch.data());

                auto flux = 0.0f;

                for (int bin = 1; bin < fftSize / 2; ++bin)
                {
                    const auto magnitude = scratch[(size_t) bin];
                    flux += juce::jmax (0.0f, magnitude - previous[(size_t) bin]);
                    previous[(size_t) bin] = magnitude;
                }

                envelope.push_back (flux);
            }

            // Mean-removed and clipped at zero: autocorrelation of a signal
            // with a large DC term correlates with its own average at every
            // lag, which buries the peak that matters.
            if (! envelope.empty())
            {
                auto mean = 0.0;
                for (const auto value : envelope)
                    mean += value;
                mean /= (double) envelope.size();

                for (auto& value : envelope)
                    value = juce::jmax (0.0f, value - (float) mean);
            }

            return envelope;
        }
    }

    Tempo detectTempo (const juce::AudioBuffer<float>& audio, double sampleRate, const Options& options)
    {
        Tempo tempo;

        if (sampleRate <= 0.0 || audio.getNumSamples() <= 0 || audio.getNumChannels() <= 0)
            return tempo;

        auto hop = hopSize;
        const auto envelope = onsetEnvelope (audio, hop);

        if (envelope.size() < 16)
            return tempo;

        const auto framesPerSecond = sampleRate / (double) hop;
        const auto shortestLag = (int) std::floor (framesPerSecond * 60.0 / options.maximumBpm);
        const auto longestLag  = (int) std::ceil  (framesPerSecond * 60.0 / options.minimumBpm);

        if (shortestLag < 1 || longestLag >= (int) envelope.size())
            return tempo;

        auto energy = 0.0;
        for (const auto value : envelope)
            energy += (double) value * (double) value;

        if (energy <= 0.0)
            return tempo;

        auto bestLag = 0;
        auto bestScore = 0.0;

        for (auto lag = shortestLag; lag <= longestLag; ++lag)
        {
            auto sum = 0.0;

            for (size_t i = (size_t) lag; i < envelope.size(); ++i)
                sum += (double) envelope[i] * (double) envelope[i - (size_t) lag];

            // Normalised by the overlap, or long lags are penalised purely
            // for having fewer terms and the search always picks the
            // fastest tempo in range.
            const auto score = sum / (double) (envelope.size() - (size_t) lag);

            if (score > bestScore)
            {
                bestScore = score;
                bestLag = lag;
            }
        }

        if (bestLag <= 0)
            return tempo;

        // Confidence: how much of the envelope's own energy the winning
        // period accounts for. A steady loop scores high; a pad, where the
        // flux is near-flat, scores near zero and is refused below.
        const auto reference = energy / (double) envelope.size();
        tempo.confidence = (float) juce::jlimit (0.0, 1.0, bestScore / juce::jmax (1.0e-12, reference));

        if (tempo.confidence < options.minimumTempoConfidence)
            return tempo;

        tempo.bpm = 60.0 * framesPerSecond / (double) bestLag;

        // Phase: slide a pulse train one beat's worth of frames and keep
        // the offset that collects the most onset energy. Without this the
        // grid has the right spacing and the wrong origin, which cuts every
        // loop the same distance *into* the bar.
        auto bestOffset = 0;
        auto bestPulse = -1.0;

        for (auto offset = 0; offset < bestLag; ++offset)
        {
            auto sum = 0.0;

            for (size_t i = (size_t) offset; i < envelope.size(); i += (size_t) bestLag)
                sum += (double) envelope[i];

            if (sum > bestPulse)
            {
                bestPulse = sum;
                bestOffset = offset;
            }
        }

        tempo.firstBeatSample = bestOffset * hop;
        tempo.detected = true;

        return tempo;
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

        // The grid, if this audio has one.
        //
        // With a tempo, cuts land on bar lines and the clip is a whole
        // number of bars long - so the loop restarts where the music
        // restarts, and the seam stops being audible however the edges are
        // faded. Without one, nothing here pretends: fixed lengths, snapped
        // to the quietest nearby point, exactly as before.
        const auto tempo = detectTempo (audio, sampleRate, options);

        auto stepSamples = sliceSamples;
        auto gridOrigin = 0;
        auto clipSamples = sliceSamples;

        if (tempo.detected)
        {
            const auto beatSamples = 60.0 / tempo.bpm * sampleRate;
            const auto barSamples = beatSamples * (double) juce::jmax (1, options.beatsPerBar);

            // Bars per clip is chosen to land nearest the requested length
            // rather than fixed, so a slow track does not produce a
            // twenty-second loop and a fast one a three-second one.
            const auto wanted = juce::jmax (1.0, options.sliceSeconds * sampleRate / barSamples);
            const auto bars = juce::jlimit (1, options.barsPerSlice * 2,
                                             (int) std::lround (wanted));

            clipSamples = (int) std::lround (barSamples * (double) bars);
            stepSamples = clipSamples;
            gridOrigin = tempo.firstBeatSample;
        }

        if (clipSamples < fftSize || audio.getNumSamples() < clipSamples)
            return slices;

        const auto searchRadius = tempo.detected
                                    ? 0   // the grid *is* the answer; nudging it off the bar undoes it
                                    : (int) (options.snapWindow * (double) sliceSamples);

        // How loud each passage is against the rest of this track, for the
        // density label - collected first because it is a comparison, and
        // a comparison needs all the members before any of them can be
        // labelled.
        std::vector<float> passageRms;

        for (auto position = gridOrigin; position + clipSamples <= audio.getNumSamples(); position += stepSamples)
            passageRms.push_back (rmsOf (monoSum (audio, position, clipSamples)));

        auto quietThreshold = 0.0f, loudThreshold = 0.0f;

        if (! passageRms.empty())
        {
            auto sorted = passageRms;
            std::sort (sorted.begin(), sorted.end());
            quietThreshold = sorted[sorted.size() / 3];
            loudThreshold = sorted[juce::jmin (sorted.size() - 1, sorted.size() * 2 / 3)];
        }

        auto passageIndex = size_t (0);

        for (auto position = gridOrigin; position + clipSamples <= audio.getNumSamples(); position += stepSamples, ++passageIndex)
        {
            const auto sliceSamples = clipSamples;
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

            // Loudness against this track's own distribution, not against
            // an absolute figure: a quiet mix's densest passage is still
            // its densest passage, and an absolute threshold would label
            // every clip of it sparse.
            slice.density = passageIndex < passageRms.size()
                              ? (passageRms[passageIndex] <= quietThreshold ? Density::sparse
                                 : passageRms[passageIndex] >= loudThreshold ? Density::dense
                                                                             : Density::moderate)
                              : Density::moderate;

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

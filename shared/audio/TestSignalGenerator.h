#pragma once

#include "shared/audio/PinkNoiseGenerator.h"
#include "shared/audio/ReferenceAudioLibrary.h"
#include "shared/audio/LessonAudioBed.h"
#include <atomic>
#include <vector>

// Drop-in replacement for PinkNoiseGenerator (same nextSample() shape, same
// call sites) used by every game except StereoWidthGame - which needs two
// independently-decorrelated sources (see StereoWidthGame.h), something a
// single real recorded file can't provide, so it deliberately keeps a plain
// PinkNoiseGenerator instead of this class.
//
// When setLibrary() has been given a ReferenceAudioLibrary with a user-
// selected file loaded, this loops that file's audio instead of synthesized
// pink noise; otherwise (no library, or no selection made yet) it behaves
// exactly as PinkNoiseGenerator always did. nextSample() only ever does an
// atomic pointer load and an array read - no allocation, no file I/O - so
// it's safe to call from the audio thread exactly like the noise generator
// it replaces.
class TestSignalGenerator
{
public:
    void setLibrary (const ReferenceAudioLibrary* newLibrary) noexcept { library = newLibrary; }

    // Only reaches the fallback noise: a player who has selected their own
    // audio has already answered the question this setting asks.
    void setNoiseColour (NoiseColour colour) noexcept { pinkNoise.setColour (colour); }

    // ---- the exercise's own sound (ADR 040) ----------------------------
    //
    // Renders a few variations of one synthesized bed (drum loop, bass note,
    // single hit...) as mono loops, levelled to the same RMS as the pink
    // noise, and plays one of them whenever the player has not picked a
    // sound of their own and has not asked for pink noise. Same level as
    // the noise on purpose: the exercises that care about input level - a
    // waveshaper, a compressor - were tuned against the noise, and a hotter
    // bed would quietly make every round a different difficulty.
    //
    // Message thread, and only while no audio is running (prepare): it
    // allocates, and the audio thread reads these buffers without a lock.
    void setExerciseBed (LessonAudioBed::Bed bed, double sampleRate, int numVariations = 4)
    {
        beds.clear();
        activeBed.store (0);

        const auto noiseRms = measurePinkRms();

        for (int v = 0; v < juce::jmax (1, numVariations); ++v)
        {
            const auto stereo = LessonAudioBed::render (bed, sampleRate, 101 + v);
            juce::AudioBuffer<float> mono (1, stereo.getNumSamples());

            auto sumOfSquares = 0.0;
            for (int i = 0; i < stereo.getNumSamples(); ++i)
            {
                auto value = stereo.getSample (0, i);
                if (stereo.getNumChannels() > 1)
                    value = 0.5f * (value + stereo.getSample (1, i));

                mono.setSample (0, i, value);
                sumOfSquares += (double) value * (double) value;
            }

            const auto rms = std::sqrt (sumOfSquares / (double) juce::jmax (1, mono.getNumSamples()));
            if (rms > 1.0e-6)
                mono.applyGain ((float) (noiseRms / rms));

            beds.push_back (std::move (mono));
        }

        bedPosition = 0;
    }

    // A different variation for the next round. Message thread; the audio
    // thread picks it up at its next sample, and every variation stays
    // allocated, so nothing is freed under it.
    void nextBedVariation (juce::Random& random) noexcept
    {
        if (beds.size() > 1)
            activeBed.store (random.nextInt ((int) beds.size()));
    }

    // Whether to play the bed rather than pink noise when nothing is
    // selected. True by default - the exercise's own sound is the better
    // teacher - and false when the player explicitly chose pink noise.
    void setPreferBed (bool shouldPrefer) noexcept { preferBed.store (shouldPrefer); }

    bool hasBed() const noexcept { return ! beds.empty(); }

    // True when the next samples are pink noise. The burst exercises shape
    // noise into hits with an envelope of their own; a bed or a clip
    // already has its rhythm and must not be chopped a second time.
    bool isPlayingNoise() const noexcept
    {
        if (library != nullptr && library->getActiveBuffer() != nullptr)
            return false;

        return ! (preferBed.load() && ! beds.empty());
    }

    float nextSample() noexcept
    {
        if (library != nullptr)
        {
            if (const auto* buffer = library->getActiveBuffer())
            {
                if (const auto length = buffer->getNumSamples(); length > 0)
                {
                    // Wrap *before* reading, not after. The library swaps in a
                    // new clip between rounds without telling anyone, and
                    // readPosition survives the swap - so a long clip followed
                    // by a short one used to read past the end of the new
                    // buffer. PracticeAudioSource has always done it this way
                    // round; this is the same discipline.
                    if (readPosition >= length)
                        readPosition = 0;

                    const auto value = buffer->getSample (0, readPosition);
                    ++readPosition;
                    return value;
                }
            }
        }

        readPosition = 0;

        if (preferBed.load (std::memory_order_relaxed) && ! beds.empty())
        {
            const auto& bed = beds[(size_t) juce::jlimit (0, (int) beds.size() - 1, activeBed.load (std::memory_order_relaxed))];
            const auto length = bed.getNumSamples();

            if (length > 0)
            {
                if (bedPosition >= length)
                    bedPosition = 0;

                return bed.getSample (0, bedPosition++);
            }
        }

        return pinkNoise.nextSample();
    }

    // What a measurement should run through: the same material the player
    // is about to hear, from a fixed starting point, without disturbing the
    // live read position. The exercises measure makeup gain on the message
    // thread, and a compressor or a waveshaper measured on pink noise is a
    // different device from the one running on a drum loop. Message thread.
    void fillForMeasurement (float* destination, int numSamples, juce::int64 seed) const
    {
        const juce::AudioBuffer<float>* source = nullptr;

        if (library != nullptr)
            source = library->getActiveBuffer();

        if (source == nullptr && preferBed.load() && ! beds.empty())
            source = &beds[(size_t) juce::jlimit (0, (int) beds.size() - 1, activeBed.load())];

        if (source != nullptr && source->getNumSamples() > 0)
        {
            const auto length = source->getNumSamples();
            for (int i = 0; i < numSamples; ++i)
                destination[i] = source->getSample (0, i % length);

            return;
        }

        PinkNoiseGenerator measuring { seed };
        measuring.setColour (pinkNoise.getColour());
        for (int i = 0; i < numSamples; ++i)
            destination[i] = measuring.nextSample();
    }

private:
    static double measurePinkRms()
    {
        PinkNoiseGenerator measuring { 0x5EED };
        auto sumOfSquares = 0.0;
        constexpr int n = 65536;

        for (int i = 0; i < n; ++i)
        {
            const auto v = (double) measuring.nextSample();
            sumOfSquares += v * v;
        }

        return std::sqrt (sumOfSquares / n);
    }

    PinkNoiseGenerator pinkNoise;
    std::vector<juce::AudioBuffer<float>> beds;
    std::atomic<int> activeBed { 0 };
    std::atomic<bool> preferBed { true };
    int bedPosition = 0;
    const ReferenceAudioLibrary* library = nullptr;
    int readPosition = 0;
};

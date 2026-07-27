#include <juce_core/juce_core.h>
#include "../Source/AudioSliceAnalyzer.h"
#include <cmath>

// Drives the analyser with signals whose right answer is known by
// construction - a click train really is percussive, a 60 Hz sine really
// is bass - so the classifier is checked against physics rather than
// against a recording someone had to listen to and label by hand.
class AudioSliceAnalyzerTest : public juce::UnitTest
{
public:
    AudioSliceAnalyzerTest() : juce::UnitTest ("AudioSliceAnalyzer") {}

    void runTest() override
    {
        constexpr double sampleRate = 44100.0;

        beginTest ("audio shorter than one slice yields nothing, rather than a runt slice");
        {
            auto shortBuffer = makeSine (sampleRate, 2.0, 440.0f);
            expect (AudioSliceAnalyzer::analyse (shortBuffer, sampleRate).empty());
        }

        beginTest ("an empty or malformed buffer is a harmless miss");
        {
            juce::AudioBuffer<float> empty;
            expect (AudioSliceAnalyzer::analyse (empty, sampleRate).empty());

            auto silent = makeSine (sampleRate, 20.0, 440.0f);
            silent.clear();
            expect (AudioSliceAnalyzer::analyse (silent, sampleRate).empty(),
                     "silence should be dropped, not sliced");

            // A zero or negative sample rate must not divide by it.
            auto fine = makeSine (sampleRate, 20.0, 440.0f);
            expect (AudioSliceAnalyzer::analyse (fine, 0.0).empty());
            expect (AudioSliceAnalyzer::analyse (fine, -44100.0).empty());
        }

        beginTest ("a long file is cut into whole slices of the requested length");
        {
            auto buffer = makeSine (sampleRate, 30.0, 800.0f);

            AudioSliceAnalyzer::Options options;
            options.sliceSeconds = 8.0;

            const auto slices = AudioSliceAnalyzer::analyse (buffer, sampleRate, options);

            expect (! slices.empty());
            expectEquals ((int) slices.size(), 3, "30s at 8s per slice is three whole slices");

            for (const auto& slice : slices)
            {
                expectEquals (slice.numSamples, (int) (8.0 * sampleRate));
                expect (slice.startSample >= 0);
                expect (slice.startSample + slice.numSamples <= buffer.getNumSamples(),
                         "a slice ran past the end of the audio");
            }
        }

        beginTest ("a click train is percussive");
        {
            auto buffer = makeClickTrain (sampleRate, 20.0, 4.0);
            const auto slices = AudioSliceAnalyzer::analyse (buffer, sampleRate);

            expect (! slices.empty());

            for (const auto& slice : slices)
            {
                expect (slice.character == AudioSliceAnalyzer::Character::percussive,
                         "four clicks a second should read as percussive");
                expect (slice.onsetsPerSecond > 1.0f);
            }
        }

        beginTest ("a low sine is bass, a high one is not");
        {
            auto low = makeSine (sampleRate, 20.0, 60.0f);
            const auto lowSlices = AudioSliceAnalyzer::analyse (low, sampleRate);

            expect (! lowSlices.empty());
            expect (lowSlices.front().character == AudioSliceAnalyzer::Character::bass);
            expect (lowSlices.front().spectralCentroidHz < 400.0f,
                     "a 60 Hz sine's centroid should be nowhere near the mids");

            auto high = makeSine (sampleRate, 20.0, 9000.0f);
            const auto highSlices = AudioSliceAnalyzer::analyse (high, sampleRate);

            expect (! highSlices.empty());
            expect (highSlices.front().character == AudioSliceAnalyzer::Character::bright);
            expect (highSlices.front().spectralCentroidHz > 4000.0f);
        }

        beginTest ("a sustained mid tone is mid range, not percussive");
        {
            // The distinction that matters most in practice: a held note
            // and a drum loop can have identical spectra and still want
            // completely different exercises.
            auto buffer = makeSine (sampleRate, 20.0, 1000.0f);
            const auto slices = AudioSliceAnalyzer::analyse (buffer, sampleRate);

            expect (! slices.empty());
            expect (slices.front().character == AudioSliceAnalyzer::Character::midRange);
            expect (slices.front().onsetsPerSecond < 2.0f,
                     "a continuous tone has no onsets to speak of");
        }

        beginTest ("stereo correlation separates a wide source from a mono one");
        {
            auto mono = makeSine (sampleRate, 20.0, 1000.0f);
            const auto monoSlices = AudioSliceAnalyzer::analyse (mono, sampleRate);
            expect (! monoSlices.empty());
            expect (monoSlices.front().stereoCorrelation > 0.95f,
                     "the same signal in both channels is correlated");

            auto wide = makeDecorrelatedNoise (sampleRate, 20.0);
            const auto wideSlices = AudioSliceAnalyzer::analyse (wide, sampleRate);
            expect (! wideSlices.empty());
            expect (wideSlices.front().stereoCorrelation < 0.5f,
                     "two independent noise sources are not correlated");
        }

        beginTest ("quiet passages are dropped rather than offered as clips");
        {
            // Loud for the first eight seconds, near-silent for the rest -
            // the shape of a track that fades out, and the reason a raw
            // "cut every 8 seconds" would fill the library with nothing.
            auto buffer = makeSine (sampleRate, 24.0, 1000.0f);
            const auto fadeStart = (int) (8.0 * sampleRate);

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.applyGain (channel, fadeStart, buffer.getNumSamples() - fadeStart, 0.001f);

            const auto slices = AudioSliceAnalyzer::analyse (buffer, sampleRate);

            expectEquals ((int) slices.size(), 1, "only the loud part is usable");
            expect (slices.front().rms > 0.02f);
        }

        beginTest ("every character maps to a distinct, non-empty folder and key");
        {
            // Folder names are the persistence: ReferenceAudioLibrary scans
            // by folder, so a duplicate would merge two categories and a
            // rename would orphan everything already sorted.
            const AudioSliceAnalyzer::Character all[] {
                AudioSliceAnalyzer::Character::percussive,
                AudioSliceAnalyzer::Character::bass,
                AudioSliceAnalyzer::Character::midRange,
                AudioSliceAnalyzer::Character::bright,
                AudioSliceAnalyzer::Character::fullRange
            };

            juce::StringArray folders, keys;

            for (const auto character : all)
            {
                const juce::String folder (AudioSliceAnalyzer::folderNameFor (character));
                const juce::String key (AudioSliceAnalyzer::nameKeyFor (character));

                expect (folder.isNotEmpty());
                expect (key.isNotEmpty());
                expect (! folders.contains (folder), "duplicate folder name: " + folder);
                expect (! keys.contains (key), "duplicate i18n key: " + key);

                folders.add (folder);
                keys.add (key);
            }
        }
    }

private:
    static juce::AudioBuffer<float> makeSine (double sampleRate, double seconds, float frequency)
    {
        const auto numSamples = (int) (sampleRate * seconds);
        juce::AudioBuffer<float> buffer (2, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto value = 0.5f * std::sin (juce::MathConstants<float>::twoPi
                                                 * frequency * (float) i / (float) sampleRate);
            buffer.setSample (0, i, value);
            buffer.setSample (1, i, value);
        }

        return buffer;
    }

    static juce::AudioBuffer<float> makeClickTrain (double sampleRate, double seconds, double clicksPerSecond)
    {
        const auto numSamples = (int) (sampleRate * seconds);
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        const auto period = (int) (sampleRate / clicksPerSecond);
        const auto decay = (int) (sampleRate * 0.03);
        juce::Random random (1234);

        for (int start = 0; start + decay < numSamples; start += period)
            for (int i = 0; i < decay; ++i)
            {
                // Filtered noise burst rather than a single impulse: an
                // impulse is a mathematical object, a hit is a short noisy
                // envelope, and only the second one exercises the flux
                // detector the way real material does.
                const auto envelope = 1.0f - (float) i / (float) decay;
                const auto value = (random.nextFloat() * 2.0f - 1.0f) * envelope * 0.7f;

                buffer.setSample (0, start + i, value);
                buffer.setSample (1, start + i, value);
            }

        return buffer;
    }

    static juce::AudioBuffer<float> makeDecorrelatedNoise (double sampleRate, double seconds)
    {
        const auto numSamples = (int) (sampleRate * seconds);
        juce::AudioBuffer<float> buffer (2, numSamples);

        juce::Random left (11), right (22);

        for (int i = 0; i < numSamples; ++i)
        {
            buffer.setSample (0, i, (left.nextFloat() * 2.0f - 1.0f) * 0.4f);
            buffer.setSample (1, i, (right.nextFloat() * 2.0f - 1.0f) * 0.4f);
        }

        return buffer;
    }
};

static AudioSliceAnalyzerTest audioSliceAnalyzerTest;

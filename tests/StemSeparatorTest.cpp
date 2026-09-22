#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "shared/audio/StemSeparator.h"
#include "shared/audio/ReferenceAudioLibrary.h"
#include <cmath>

// Drives the separator with signals whose right answer is known by
// construction: a click train is percussive, a held 60 Hz sine is bass, a
// tone identical in both speakers is centre, two unrelated tones one per
// speaker are wide. None of this proves it can pull a vocal out of a real
// mix - nothing short of listening does - but it pins down that each stage
// sends each kind of energy where the header says it goes, and that
// nothing is lost on the way.
class StemSeparatorTest : public juce::UnitTest
{
public:
    StemSeparatorTest() : juce::UnitTest ("StemSeparator") {}

    void runTest() override
    {
        constexpr double sampleRate = 44100.0;
        using StemSeparator::Stem;

        beginTest ("a click train lands mostly in drums");
        {
            const auto input = makeClickTrain (sampleRate, 10.0, 2.0);
            const auto result = StemSeparator::separate (input, sampleRate);
            expect (result.completed);

            const auto share = shareOf (result, Stem::drums);
            expect (share > 0.7, "drums share of a click train: " + juce::String (share));
            logMessage (juce::String ("drums share of a click train: ") + juce::String (share, 3));
        }

        beginTest ("a sustained 60 Hz sine lands mostly in bass");
        {
            const auto input = makeTones (sampleRate, 10.0, 60.0, 60.0);
            const auto result = StemSeparator::separate (input, sampleRate);

            const auto share = shareOf (result, Stem::bass);
            expect (share > 0.85, "bass share of a 60 Hz sine: " + juce::String (share));
            logMessage (juce::String ("bass share of a 60 Hz sine: ") + juce::String (share, 3));
        }

        beginTest ("a sustained 1 kHz tone identical in both channels lands mostly in centre");
        {
            const auto input = makeTones (sampleRate, 10.0, 1000.0, 1000.0);
            const auto result = StemSeparator::separate (input, sampleRate);

            const auto share = shareOf (result, Stem::centre);
            expect (share > 0.85, "centre share of a centred tone: " + juce::String (share));
            logMessage (juce::String ("centre share of a centred tone: ") + juce::String (share, 3));
        }

        beginTest ("unrelated tones in each speaker land mostly in sides");
        {
            // 1 kHz only on the left, 1.3 kHz only on the right: in every
            // bin that holds energy, one speaker has it and the other does
            // not. That is as wide as a signal gets, not a trick of level.
            const auto input = makeTones (sampleRate, 10.0, 1000.0, 1300.0);
            const auto result = StemSeparator::separate (input, sampleRate);

            const auto share = shareOf (result, Stem::sides);
            expect (share > 0.85, "sides share of hard-panned tones: " + juce::String (share));
            logMessage (juce::String ("sides share of hard-panned tones: ") + juce::String (share, 3));
        }

        beginTest ("independent noise in each channel reads as wide, not centre");
        {
            // Equal level in both speakers, so a per-frame level comparison
            // would call it centred - only the time-averaged coherence can
            // tell it is decorrelated.
            juce::AudioBuffer<float> input (2, (int) (sampleRate * 6.0));
            juce::Random random (7);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < input.getNumSamples(); ++i)
                    input.setSample (ch, i, (random.nextFloat() * 2.0f - 1.0f) * 0.3f);

            const auto result = StemSeparator::separate (input, sampleRate);

            const auto centre = energyOf (result.get (Stem::centre));
            const auto sides = energyOf (result.get (Stem::sides));
            expect (sides > centre * 3.0, "sides " + juce::String (sides) + " vs centre " + juce::String (centre));
            logMessage ("decorrelated noise, sides/centre energy ratio: " + juce::String (sides / juce::jmax (1.0e-30, centre), 1));
        }

        beginTest ("the stems add back up to the input");
        {
            auto input = makeClickTrain (sampleRate, 7.3, 3.0);
            const auto tones = makeTones (sampleRate, 7.3, 1000.0, 1300.0);
            const auto bass = makeTones (sampleRate, 7.3, 55.0, 55.0);

            juce::Random random (3);
            for (int ch = 0; ch < 2; ++ch)
            {
                input.addFrom (ch, 0, tones, ch, 0, input.getNumSamples());
                input.addFrom (ch, 0, bass, ch, 0, input.getNumSamples());

                for (int i = 0; i < input.getNumSamples(); ++i)
                    input.addSample (ch, i, (random.nextFloat() * 2.0f - 1.0f) * 0.05f);
            }

            const auto result = StemSeparator::separate (input, sampleRate);
            const auto errorDb = reconstructionErrorDb (input, result);
            expect (errorDb < -30.0, "reconstruction error " + juce::String (errorDb) + " dB");
            logMessage ("stereo reconstruction error: " + juce::String (errorDb, 1) + " dB");

            // And every stem really did get something - the sum test alone
            // would pass for a separator that put everything in one stem.
            for (int s = 0; s < StemSeparator::numStems; ++s)
                expect (energyOf (result.stems[(size_t) s]) > 0.0,
                         juce::String ("stem is empty: ") + StemSeparator::folderNameFor ((Stem) s));
        }

        beginTest ("mono input: sides are silent, everything else still adds up");
        {
            auto input = makeTones (sampleRate, 6.0, 1000.0, 1000.0);
            const auto clicks = makeClickTrain (sampleRate, 6.0, 2.0);
            input.addFrom (0, 0, clicks, 0, 0, input.getNumSamples());
            input.setSize (1, input.getNumSamples(), true);

            const auto result = StemSeparator::separate (input, sampleRate);

            for (const auto& stem : result.stems)
                expectEquals (stem.getNumChannels(), 1);

            const auto& sides = result.get (Stem::sides);
            expect (sides.getMagnitude (0, 0, sides.getNumSamples()) < 1.0e-6f,
                     "a mono file has no stereo position to sort by - sides must be silence");

            expect (energyOf (result.get (Stem::centre)) > 0.0);
            expect (reconstructionErrorDb (input, result) < -30.0);
        }

        beginTest ("empty and very short input is harmless");
        {
            juce::AudioBuffer<float> empty;
            auto result = StemSeparator::separate (empty, sampleRate);
            expect (result.completed);
            expectEquals (result.get (Stem::drums).getNumSamples(), 0);

            auto fine = makeTones (sampleRate, 1.0, 440.0, 440.0);
            expectEquals (StemSeparator::separate (fine, 0.0).get (Stem::centre).getNumSamples(), 0);
            expectEquals (StemSeparator::separate (fine, -44100.0).get (Stem::centre).getNumSamples(), 0);

            // Shorter than one FFT window, and a single sample.
            for (const auto length : { 10, 1 })
            {
                juce::AudioBuffer<float> tiny (2, length);
                for (int i = 0; i < length; ++i)
                {
                    tiny.setSample (0, i, 0.5f);
                    tiny.setSample (1, i, -0.25f);
                }

                result = StemSeparator::separate (tiny, sampleRate);
                expect (result.completed);
                expectEquals (result.get (Stem::sides).getNumSamples(), length);
                expect (reconstructionErrorDb (tiny, result) < -30.0,
                         "short input did not reconstruct, length " + juce::String (length));
            }

            // Silence stays silence, and does not divide by zero on the way.
            juce::AudioBuffer<float> silent (2, (int) sampleRate);
            silent.clear();
            result = StemSeparator::separate (silent, sampleRate);

            for (const auto& stem : result.stems)
                for (int ch = 0; ch < stem.getNumChannels(); ++ch)
                    expectEquals (stem.getMagnitude (ch, 0, stem.getNumSamples()), 0.0f);
        }

        beginTest ("shouldStop abandons the work and reports it");
        {
            const auto input = makeTones (sampleRate, 5.0, 440.0, 440.0);
            auto progressCalls = 0;
            const auto result = StemSeparator::separate (input, sampleRate, {},
                                                         [&] (float) { ++progressCalls; },
                                                         [] { return true; });
            expect (! result.completed);
            expectEquals (result.get (Stem::drums).getNumSamples(), 0);
        }

        // Opt-in: measures a three-minute stereo song's worth, which is too
        // slow to pay on every test run.
        if (juce::SystemStats::getEnvironmentVariable ("ABC_STEM_BENCH", {}).isNotEmpty())
        {
            beginTest ("benchmark: three minutes of stereo");

            auto input = makeTones (sampleRate, 180.0, 1000.0, 1300.0);
            juce::Random random (11);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < input.getNumSamples(); ++i)
                    input.addSample (ch, i, (random.nextFloat() * 2.0f - 1.0f) * 0.1f);

            const auto started = juce::Time::getMillisecondCounterHiRes();
            const auto result = StemSeparator::separate (input, sampleRate);
            const auto seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;

            expect (result.completed);
            logMessage ("3 min stereo 44.1 kHz separated in " + juce::String (seconds, 2) + " s");
        }

        beginTest ("importAndSeparateMany files clips into the four stem folders");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("abcTrainStemImportTest").getNonexistentSibling();
            root.createDirectory();

            // Everything at once, each at a level the slicer will keep:
            // drums, a bass note, a centred tone, and two hard-panned tones.
            const auto seconds = 30.0;
            auto mix = makeClickTrain (sampleRate, seconds, 2.0);
            auto bass = makeTones (sampleRate, seconds, 60.0, 60.0);
            auto centre = makeTones (sampleRate, seconds, 1000.0, 1000.0);
            auto wide = makeTones (sampleRate, seconds, 1500.0, 2100.0);

            for (int ch = 0; ch < 2; ++ch)
            {
                mix.addFrom (ch, 0, bass, ch, 0, mix.getNumSamples(), 0.5f);
                mix.addFrom (ch, 0, centre, ch, 0, mix.getNumSamples(), 0.5f);
                mix.addFrom (ch, 0, wide, ch, 0, mix.getNumSamples(), 0.5f);
            }

            const auto source = writeWav (root.getChildFile ("song.wav"), mix, sampleRate);
            const auto sourceSize = source.getSize();

            // The same song in mono: separation still works, but there are
            // no sides to find.
            juce::AudioBuffer<float> mono (1, mix.getNumSamples());
            mono.clear();
            mono.addFrom (0, 0, mix, 0, 0, mix.getNumSamples(), 0.5f);
            mono.addFrom (0, 0, mix, 1, 0, mix.getNumSamples(), 0.5f);
            const auto monoRoot = root.getChildFile ("mono");
            const auto monoSource = writeWav (root.getChildFile ("mono song.wav"), mono, sampleRate);

            auto options = makeTempOptions ("stems");
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary library (properties);
            library.setRootFolder (root);

            auto lastProgress = -1.0f;
            auto progressWentBackwards = false;

            const auto written = library.importAndSeparateMany (
                { source },
                [&] (float p, juce::String)
                {
                    progressWentBackwards = progressWentBackwards || p < lastProgress;
                    lastProgress = p;
                },
                [] { return false; });

            expect (written > 0, "nothing was written");
            expect (! progressWentBackwards, "progress went backwards");
            expectEquals (lastProgress, 1.0f);

            auto onDisk = 0;

            for (int s = 0; s < StemSeparator::numStems; ++s)
            {
                const auto folder = root.getChildFile (StemSeparator::folderNameFor ((StemSeparator::Stem) s));
                const auto clips = folder.findChildFiles (juce::File::findFiles, false, "*.wav");

                expect (clips.size() > 0, juce::String ("no clips in ") + folder.getFileName());
                onDisk += clips.size();

                juce::AudioFormatManager formats;
                formats.registerBasicFormats();

                for (const auto& clip : clips)
                {
                    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (clip));
                    expect (reader != nullptr, "unreadable clip " + clip.getFullPathName());

                    if (reader != nullptr)
                    {
                        expectEquals ((int) reader->numChannels, 2);
                        expect (reader->lengthInSamples > (juce::int64) (sampleRate * 4.0));
                    }
                }
            }

            expectEquals (onDisk, written, "the count reported does not match the files on disk");
            expect (source.existsAsFile() && source.getSize() == sourceSize, "the source file was disturbed");

            // The ordinary slice folders are not written by a stem import.
            expect (! root.getChildFile ("Percussive").exists());

            // Mono, into its own root.
            monoRoot.createDirectory();
            library.setRootFolder (monoRoot);
            const auto monoWritten = library.importAndSeparateMany ({ monoSource }, nullptr, nullptr);

            expect (monoWritten > 0);
            expect (! monoRoot.getChildFile (StemSeparator::folderNameFor (Stem::sides))
                          .findChildFiles (juce::File::findFiles, false, "*.wav").size(),
                     "a mono source produced sides clips");
            expect (monoRoot.getChildFile (StemSeparator::folderNameFor (Stem::centre)).isDirectory());

            // Junk, a folder and a missing file are skipped, not fatal; a
            // stop request before anything starts writes nothing.
            const auto junk = root.getChildFile ("junk.wav");
            junk.replaceWithText ("not audio");
            expectEquals (library.importAndSeparateMany ({ junk, root, root.getChildFile ("missing.wav") },
                                                         nullptr, nullptr), 0);
            expectEquals (library.importAndSeparateMany ({ source }, nullptr, [] { return true; }), 0);

            root.deleteRecursively();
            options.getDefaultFile().deleteFile();
        }
    }

private:
    static juce::AudioBuffer<float> makeTones (double sampleRate, double seconds, double leftHz, double rightHz)
    {
        const auto numSamples = (int) (sampleRate * seconds);
        juce::AudioBuffer<float> buffer (2, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto t = (double) i / sampleRate;
            buffer.setSample (0, i, (float) (0.5 * std::sin (juce::MathConstants<double>::twoPi * leftHz * t)));
            buffer.setSample (1, i, (float) (0.5 * std::sin (juce::MathConstants<double>::twoPi * rightHz * t)));
        }

        return buffer;
    }

    // Short decaying noise bursts, identical in both channels - the same
    // construction AudioSliceAnalyzerTest uses for "a hit".
    static juce::AudioBuffer<float> makeClickTrain (double sampleRate, double seconds, double clicksPerSecond)
    {
        const auto numSamples = (int) (sampleRate * seconds);
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        const auto period = (int) (sampleRate / clicksPerSecond);
        const auto decay = (int) (sampleRate * 0.03);
        juce::Random random (1234);

        for (int start = period / 4; start + decay < numSamples; start += period)
            for (int i = 0; i < decay; ++i)
            {
                const auto envelope = 1.0f - (float) i / (float) decay;
                const auto value = (random.nextFloat() * 2.0f - 1.0f) * envelope * 0.7f;
                buffer.setSample (0, start + i, value);
                buffer.setSample (1, start + i, value);
            }

        return buffer;
    }

    static double energyOf (const juce::AudioBuffer<float>& buffer)
    {
        auto sum = 0.0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                sum += (double) buffer.getSample (ch, i) * (double) buffer.getSample (ch, i);
        return sum;
    }

    static double shareOf (const StemSeparator::Result& result, StemSeparator::Stem stem)
    {
        auto total = 0.0;
        for (const auto& s : result.stems)
            total += energyOf (s);

        return total > 0.0 ? energyOf (result.get (stem)) / total : 0.0;
    }

    static double reconstructionErrorDb (const juce::AudioBuffer<float>& input, const StemSeparator::Result& result)
    {
        auto signal = 0.0, error = 0.0;

        for (int ch = 0; ch < input.getNumChannels(); ++ch)
            for (int i = 0; i < input.getNumSamples(); ++i)
            {
                auto sum = 0.0;
                for (const auto& s : result.stems)
                    sum += (double) s.getSample (ch, i);

                const auto x = (double) input.getSample (ch, i);
                signal += x * x;
                error += (sum - x) * (sum - x);
            }

        return 10.0 * std::log10 ((error + 1.0e-30) / (signal + 1.0e-30));
    }

    static juce::File writeWav (const juce::File& destination, const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        destination.deleteFile();

        if (auto stream = destination.createOutputStream())
        {
            juce::WavAudioFormat wav;

            if (auto* writer = wav.createWriterFor (stream.get(), sampleRate,
                                                    (unsigned int) buffer.getNumChannels(), 16, {}, 0))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
                delete writer;
            }
        }

        return destination;
    }

    static juce::PropertiesFile::Options makeTempOptions (const juce::String& uniqueSuffix)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "EarTrainerTests";
        options.filenameSuffix = "settings";
        options.folderName = "EarTrainerTests_stemseparator_" + uniqueSuffix;
        options.commonToAllUsers = false;
        options.getDefaultFile().deleteFile();
        return options;
    }
};

static StemSeparatorTest stemSeparatorTest;

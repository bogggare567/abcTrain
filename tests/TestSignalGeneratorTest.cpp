#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "../shared/ReferenceAudioLibrary.h"
#include "../shared/TestSignalGenerator.h"

// The library swaps the clip under the audio thread between rounds, and the
// generator's read position survives that swap. So the question this file
// exists to ask is: what happens when the new clip is SHORTER than the
// position we are already at?
//
// It used to read the sample first and wrap afterwards, which meant a
// 1.8-second clip followed by a 1.0-second one read tens of thousands of
// floats past the end of the new buffer - on the audio thread, in a release
// build, where AudioBuffer::getSample is a raw pointer dereference. Audible
// as a click on the round transition; occasionally a NaN into an IIR filter
// with no guard, which takes the rest of the round with it.
//
// None of that is something a crash test would reliably catch, so this
// checks the property instead: every sample handed out is a sample that
// genuinely exists in the buffer currently selected.
class TestSignalGeneratorTest : public juce::UnitTest
{
public:
    TestSignalGeneratorTest() : juce::UnitTest ("TestSignalGenerator", "Games") {}

    void runTest() override
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("abcTrainSignalGenTest");
        root.deleteRecursively();
        root.createDirectory();

        // Two clips whose samples identify themselves: every sample of the
        // long one is -1, every sample of the short one is +1. That is what
        // makes a stale read detectable rather than merely undefined.
        const auto category = root.getChildFile ("Clips");
        writeConstantWav (category.getChildFile ("long.wav"),  44100.0, 44100, -1.0f);
        writeConstantWav (category.getChildFile ("short.wav"), 44100.0,  4410, +1.0f);

        auto properties = makeProperties();
        ReferenceAudioLibrary library { *properties };
        library.setRootFolder (root);
        library.prepare (44100.0);

        beginTest ("a shorter clip after a longer one is still read in bounds");
        {
            TestSignalGenerator generator;
            generator.setLibrary (&library);

            expect (library.selectFile (category.getChildFile ("long.wav"), 44100.0),
                     "could not select the long clip");

            // Walk most of the way through the long clip, so the read
            // position is far past the end of the short one.
            for (int i = 0; i < 40000; ++i)
                generator.nextSample();

            expect (library.selectFile (category.getChildFile ("short.wav"), 44100.0),
                     "could not select the short clip");

            // Every sample from here on must come from the short clip, which
            // is entirely +1. A stale position reads whatever the allocator
            // left behind - almost never exactly +1.
            auto worst = 0.0f;

            for (int i = 0; i < 20000; ++i)
                worst = juce::jmax (worst, std::abs (generator.nextSample() - 1.0f));

            expect (worst < 1.0e-3f,
                     "read outside the selected clip - worst deviation from +1 was "
                         + juce::String (worst, 6));
        }

        beginTest ("the position wraps rather than running off the end");
        {
            // Same shape, no swap: simply reading far more samples than the
            // clip holds must stay in bounds and keep returning the clip.
            TestSignalGenerator generator;
            generator.setLibrary (&library);

            expect (library.selectFile (category.getChildFile ("short.wav"), 44100.0),
                     "could not select the short clip");

            auto worst = 0.0f;

            for (int i = 0; i < 4410 * 7; ++i)
                worst = juce::jmax (worst, std::abs (generator.nextSample() - 1.0f));

            expect (worst < 1.0e-3f,
                     "looping past the end left the buffer - worst deviation was "
                         + juce::String (worst, 6));
        }

        beginTest ("with no library at all it still makes noise");
        {
            // The fallback path matters: this is what every exercise uses
            // until the player picks their own audio.
            TestSignalGenerator generator;
            auto sawSomething = false;

            for (int i = 0; i < 4096; ++i)
                if (std::abs (generator.nextSample()) > 1.0e-4f)
                    sawSomething = true;

            expect (sawSomething, "no reference library and no pink noise either");
        }

        root.deleteRecursively();
    }

private:
    static std::unique_ptr<juce::PropertiesFile> makeProperties()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "abcTrainSignalGenTest";
        options.filenameSuffix = "settings";
        options.folderName = "abcTrainSignalGenTest";
        options.osxLibrarySubFolder = "Application Support";
        return std::make_unique<juce::PropertiesFile> (options);
    }

    static void writeConstantWav (const juce::File& destination, double sampleRate,
                                   int numSamples, float value)
    {
        destination.getParentDirectory().createDirectory();
        destination.deleteFile();

        juce::AudioBuffer<float> buffer (1, numSamples);

        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (0, i, value);

        juce::WavAudioFormat wav;

        if (auto stream = destination.createOutputStream())
        {
            if (auto* writer = wav.createWriterFor (stream.get(), sampleRate, 1, 16, {}, 0))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
                delete writer;
            }
        }
    }
};

static TestSignalGeneratorTest testSignalGeneratorTest;

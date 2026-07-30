#include <juce_audio_basics/juce_audio_basics.h>
#include "../shared/PracticeAudioSource.h"

// What matters here is that a plugin cannot start making noise on its own,
// and that turning it on and off does not click. Both are things a user
// would notice immediately and neither is visible by reading the code:
// "off by default" is one atomic's initialiser, and a click is a gain step
// nobody wrote down.
class PracticeAudioSourceTest : public juce::UnitTest
{
public:
    PracticeAudioSourceTest() : juce::UnitTest ("PracticeAudioSource") {}

    void runTest() override
    {
        // A real ReferenceAudioLibrary needs a PropertiesFile and a disk;
        // everything under test here reads it through one method, so a
        // temporary file in the test's own folder keeps it honest without
        // dragging in the file scanning.
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("abcTrain-practice-source-test");
        folder.createDirectory();

        juce::PropertiesFile::Options options;
        options.applicationName = "PracticeAudioSourceTest";
        options.filenameSuffix = ".settings";
        options.folderName = folder.getFullPathName();
        options.storageFormat = juce::PropertiesFile::storeAsXML;

        juce::PropertiesFile properties (options);
        ReferenceAudioLibrary library (properties);
        PracticeAudioSource source (library);
        source.prepare (44100.0);

        beginTest ("silent and inert until asked");
        {
            expect (! source.isEnabled());

            juce::AudioBuffer<float> buffer (2, 512);
            fillWith (buffer, 0.25f);

            expect (! source.fillBlock (buffer),
                    "a disabled source must not report having played");

            // The host's audio has to come through untouched - anything
            // else means the plugin edited a track it was not asked to.
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                    expectEquals (buffer.getSample (channel, sample), 0.25f);
        }

        beginTest ("enabling with nothing selected still leaves the host alone");
        {
            source.setEnabled (true);
            expect (source.isEnabled());

            juce::AudioBuffer<float> buffer (2, 256);
            fillWith (buffer, -0.5f);

            // Built-in categories exist but nothing has been *selected*, so
            // there is no active buffer. The source must degrade to a
            // passthrough rather than to silence.
            if (library.getActiveBuffer() == nullptr)
            {
                expect (! source.fillBlock (buffer));
                expectEquals (buffer.getSample (0, 0), -0.5f);
            }

            source.setEnabled (false);
        }

        beginTest ("a disabled source is exactly at rest, not nearly");
        {
            // Half a second of blocks is far more than the 30 ms fade, so
            // anything still moving after it is a gain that never settles -
            // which would leave the host's audio permanently scaled.
            juce::AudioBuffer<float> buffer (1, 512);

            for (int block = 0; block < 43; ++block)
            {
                fillWith (buffer, 1.0f);
                source.fillBlock (buffer);
            }

            expectEquals (buffer.getSample (0, 511), 1.0f);
        }

        beginTest ("a published bed outlives the caller that made it");
        {
            // The crash this guards against: the module panel belongs to
            // the editor, and a host destroys the editor whenever the
            // window is closed while audio keeps running. When the panel
            // owned the beds, closing the window during a check freed the
            // buffer out from under a block already in flight.
            //
            // Modelled here by letting the caller's buffer go out of
            // scope entirely and then asking for audio: if ownership did
            // not transfer, this reads freed memory.
            PracticeAudioSource source (library);
            source.prepare (44100.0);

            {
                juce::AudioBuffer<float> bed (1, 2048);
                for (int i = 0; i < bed.getNumSamples(); ++i)
                    bed.setSample (0, i, 0.75f);

                source.publishOverrideBuffer (std::move (bed));
            }   // the caller's buffer is gone from here on

            juce::AudioBuffer<float> buffer (2, 512);

            // Long enough for the 30 ms enable fade to finish, so the
            // sample value is the bed's own rather than a fraction of it.
            for (int block = 0; block < 10; ++block)
            {
                fillWith (buffer, 0.0f);
                expect (source.fillBlock (buffer), "an override plays even with nothing enabled");
            }

            expectWithinAbsoluteError (buffer.getSample (0, 511), 0.75f, 0.02f);

            // Clearing stops playback without freeing anything: a bed the
            // audio thread may still be inside must not be destroyed.
            source.clearOverrideBuffer();

            for (int block = 0; block < 10; ++block)
            {
                fillWith (buffer, 0.0f);
                source.fillBlock (buffer);
            }

            expectWithinAbsoluteError (buffer.getSample (0, 511), 0.0f, 0.0001f);
        }

        folder.deleteRecursively();
    }

private:
    static void fillWith (juce::AudioBuffer<float>& buffer, float value)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            juce::FloatVectorOperations::fill (buffer.getWritePointer (channel), value,
                                                buffer.getNumSamples());
    }
};

static PracticeAudioSourceTest practiceAudioSourceTest;

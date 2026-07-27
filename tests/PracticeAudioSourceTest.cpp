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

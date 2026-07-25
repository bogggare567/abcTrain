#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/ReferenceAudioLibrary.h"
#include "../shared/TestUtils.h"

namespace
{
    // Writes a short real WAV file to disk so ReferenceAudioLibrary has
    // something genuinely readable to scan/load in these tests - a fake
    // extension with garbage bytes wouldn't exercise createReaderFor() the
    // way a real file the user's own folder would contain does.
    juce::File writeTestWav (const juce::File& destination, double sampleRate, int numSamples)
    {
        destination.getParentDirectory().createDirectory();
        destination.deleteFile();

        juce::WavAudioFormat wavFormat;
        auto stream = destination.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wavFormat.createWriterFor (stream.get(), sampleRate, 1, 16, {}, 0));

        if (writer != nullptr)
        {
            stream.release(); // writer now owns it
            const auto buffer = TestUtils::generateSineBuffer (440.0f, sampleRate, numSamples);
            writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
        }

        return destination;
    }

    juce::PropertiesFile::Options makeTempOptions (const juce::String& uniqueSuffix)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "EarTrainerTests";
        options.filenameSuffix = "settings";
        options.folderName = "EarTrainerTests_referenceaudio_" + uniqueSuffix;
        options.commonToAllUsers = false;
        options.getDefaultFile().deleteFile(); // see ProgressManagerTest's identical comment on why
        return options;
    }
}

class ReferenceAudioLibraryTest : public juce::UnitTest
{
public:
    ReferenceAudioLibraryTest() : juce::UnitTest ("ReferenceAudioLibrary", "Progress") {}

    void runTest() override
    {
        const auto tempRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                   .getChildFile ("EarTrainerTests_ReferenceAudio_" + juce::String (juce::Random::getSystemRandom().nextInt64()));
        tempRoot.deleteRecursively();
        tempRoot.createDirectory();

        beginTest ("rescan() on a folder with no subfolders still yields the built-in categories");
        {
            auto options = makeTempOptions ("empty");
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary library (properties);

            library.setRootFolder (tempRoot.getChildFile ("nonexistent"));

            // No filesystem folder configured/found - but the bundled,
            // programmatically-synthesized samples (see decisions/018)
            // are always present, so this is never actually empty.
            const auto& categories = library.getCategories();
            expectEquals (categories.size(), 2);
            expectEquals (categories.getReference (0).name, juce::String ("Built-in Percussive"));
            expectEquals (categories.getReference (1).name, juce::String ("Built-in Sustained"));
        }

        beginTest ("rescan() only counts filesystem subfolders that contain real audio files, alongside the built-ins");
        {
            const auto rockDir = tempRoot.getChildFile ("Rock");
            writeTestWav (rockDir.getChildFile ("track1.wav"), 44100.0, 4410);
            writeTestWav (rockDir.getChildFile ("track2.wav"), 44100.0, 4410);

            const auto emptyDir = tempRoot.getChildFile ("EmptyGenre");
            emptyDir.createDirectory();
            emptyDir.getChildFile ("notes.txt").replaceWithText ("not audio");

            auto options = makeTempOptions ("scan");
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary library (properties);
            library.setRootFolder (tempRoot);

            const auto& categories = library.getCategories();
            // 2 built-in + "Rock" - "EmptyGenre" has no real audio in it.
            expectEquals (categories.size(), 3);

            bool foundRock = false;
            for (const auto& category : categories)
                if (category.name == "Rock")
                {
                    foundRock = true;
                    expectEquals (category.files.size(), 2);
                }
            expect (foundRock);
        }

        beginTest ("built-in categories are always unlocked-first and actually selectable");
        {
            auto options = makeTempOptions ("builtin");
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary library (properties);
            library.setRootFolder (tempRoot.getChildFile ("nonexistent"));

            const auto& categories = library.getCategories();
            expectEquals (categories.size(), 2);
            expect (categories.getReference (0).files.size() == 2); // Kick, Snare
            expect (categories.getReference (1).files.size() == 3); // Pad, Pluck, Tone

            const auto& firstFile = categories.getReference (0).files.getReference (0);
            expect (firstFile.existsAsFile());
            expect (library.selectFile (firstFile, 44100.0));
            expect (library.getActiveBuffer() != nullptr);
            expect (library.getActiveBuffer()->getNumSamples() > 0);
        }

        beginTest ("selectFile loads a real file as the active buffer; clearSelection falls back to nullptr");
        {
            const auto file = writeTestWav (tempRoot.getChildFile ("Solo/lead.wav"), 44100.0, 8820);

            auto options = makeTempOptions ("select");
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary library (properties);

            expect (library.getActiveBuffer() == nullptr);

            const auto ok = library.selectFile (file, 44100.0);
            expect (ok);
            expect (library.getActiveBuffer() != nullptr);
            expect (library.getActiveBuffer()->getNumSamples() > 0);
            expect (library.getSelectedFile() == file);

            library.clearSelection();
            expect (library.getActiveBuffer() == nullptr);
            expect (library.getSelectedFile() == juce::File());
        }

        beginTest ("selectFile resamples when the target rate differs from the file's own rate");
        {
            const auto file = writeTestWav (tempRoot.getChildFile ("Resample/track.wav"), 44100.0, 44100);

            auto options = makeTempOptions ("resample");
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary library (properties);

            expect (library.selectFile (file, 48000.0));

            const auto* buffer = library.getActiveBuffer();
            expect (buffer != nullptr);
            // 1 second of 44.1kHz audio resampled to 48kHz should be
            // close to 48000 samples, not the original 44100 - a loose
            // tolerance since LagrangeInterpolator's exact output length
            // depends on its internal filter state.
            expect (std::abs (buffer->getNumSamples() - 48000) < 200);
        }

        beginTest ("selectFile on a non-audio file fails without disturbing any existing selection");
        {
            const auto goodFile = writeTestWav (tempRoot.getChildFile ("Fallback/good.wav"), 44100.0, 4410);
            const auto badFile = tempRoot.getChildFile ("Fallback/not_audio.wav");
            badFile.replaceWithText ("this is not a real wav file");

            auto options = makeTempOptions ("badfile");
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary library (properties);

            expect (library.selectFile (goodFile, 44100.0));
            const auto* bufferBeforeFailedSelect = library.getActiveBuffer();

            expect (! library.selectFile (badFile, 44100.0));
            expect (library.getSelectedFile() == goodFile);
            expect (library.getActiveBuffer() == bufferBeforeFailedSelect);
        }

        beginTest ("root folder and selection persist across reconstruction");
        {
            const auto file = writeTestWav (tempRoot.getChildFile ("Persisted/song.wav"), 44100.0, 4410);
            const auto options = makeTempOptions ("persistence");

            {
                juce::PropertiesFile properties (options);
                ReferenceAudioLibrary library (properties);
                library.setRootFolder (tempRoot);
                library.selectFile (file, 44100.0);
            } // destructor flushes PropertiesFile

            juce::PropertiesFile reloadedProperties (options);
            ReferenceAudioLibrary reloaded (reloadedProperties);

            expect (reloaded.getRootFolder() == tempRoot);
            expect (reloaded.getSelectedFile() == file);
            expect (reloaded.getActiveBuffer() == nullptr); // not loaded until prepare()

            reloaded.prepare (44100.0);
            expect (reloaded.getActiveBuffer() != nullptr);
        }

        tempRoot.deleteRecursively();
    }
};

static ReferenceAudioLibraryTest referenceAudioLibraryTest;

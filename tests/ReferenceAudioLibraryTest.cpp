#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "shared/audio/ReferenceAudioLibrary.h"
#include "shared/audio/AudioSliceAnalyzer.h"
#include "shared/audio/InstrumentLabel.h"
#include "TestUtils.h"

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

        beginTest ("importAndSlice cuts a long file into sorted, playable clips");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("abcTrainImportTest").getNonexistentSibling();
            root.createDirectory();

            // A 30-second source: 4 clicks a second, which is
            // unambiguously percussive by construction.
            // Named like a percussion loop, and sounding like one: the name
            // picks the instrument, the sound does not argue.
            const auto source = root.getChildFile ("perc loop.wav");
            writeClickTrain (source, 44100.0, 30.0, 4.0);

            auto options = makeTempOptions ("import");
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary library (properties);
            library.setRootFolder (root);

            const auto written = library.importAndSlice (source);

            expect (written > 0, "nothing was written from a perfectly usable file");

            const auto percussive = root.getChildFile (
                InstrumentLabel::folderNameFor (InstrumentLabel::Instrument::percussion));

            expect (percussive.isDirectory(), "the instrument folder was not created");

            const auto clips = percussive.findChildFiles (juce::File::findFiles, false, "*.wav");
            expectEquals (clips.size(), written, "the count reported does not match the files on disk");

            // Every clip must be readable audio of the expected length -
            // a writer that silently produced a 44-byte header would still
            // have "written" something.
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();

            for (const auto& clip : clips)
            {
                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (clip));

                expect (reader != nullptr, "a written clip is not readable: " + clip.getFileName());

                if (reader != nullptr)
                {
                    const auto seconds = (double) reader->lengthInSamples / reader->sampleRate;
                    expect (seconds > 7.0 && seconds < 9.0,
                             "a clip is not the requested length: " + juce::String (seconds));
                }
            }

            // The source is the player's own file and must come back
            // untouched - not moved, not renamed, not consumed.
            expect (source.existsAsFile(), "the source file was disturbed");

            root.deleteRecursively();
        }

        beginTest ("importing something that is not audio is a no-op, not a crash");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("abcTrainImportJunk").getNonexistentSibling();
            root.createDirectory();

            const auto junk = root.getChildFile ("notes.txt");
            junk.replaceWithText ("this is not a wav file");

            auto options = makeTempOptions ("importjunk");
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary library (properties);
            library.setRootFolder (root);

            expectEquals (library.importAndSlice (junk), 0);
            expectEquals (library.importAndSlice (juce::File()), 0);
            expectEquals (library.importAndSlice (root.getChildFile ("nothing here.wav")), 0);

            root.deleteRecursively();
        }

        beginTest ("a category rotates through its clips instead of repeating one");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("abcTrainRotate").getNonexistentSibling();

            const auto category = root.getChildFile ("Loops");
            category.createDirectory();

            for (int i = 0; i < 5; ++i)
                writeClickTrain (category.getChildFile ("loop" + juce::String (i) + ".wav"),
                                  44100.0, 1.0, 4.0);

            auto options = makeTempOptions ("rotate");
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary library (properties);
            library.setRootFolder (root);

            library.setActiveCategory ("Loops", 44100.0);
            expect (library.getSelectedFile().existsAsFile(), "nothing was selected");

            // The point of holding a category rather than a file: a fresh
            // clip each round. Twenty advances over five files should touch
            // more than one of them - and must never repeat immediately,
            // which reads as the app being stuck.
            juce::StringArray seen;
            auto previous = library.getSelectedFile();

            for (int i = 0; i < 20; ++i)
            {
                library.advanceToRandomClip (44100.0);
                const auto current = library.getSelectedFile();

                expect (current != previous, "the same clip was played twice running");
                seen.addIfNotAlreadyThere (current.getFileName());
                previous = current;
            }

            expect (seen.size() > 1, "the rotation never left the first clip");

            // A category with one file has nowhere to rotate to, and must
            // simply stay put rather than clearing the selection.
            const auto single = root.getChildFile ("Single");
            single.createDirectory();
            writeClickTrain (single.getChildFile ("only.wav"), 44100.0, 1.0, 4.0);
            library.rescan();

            library.setActiveCategory ("Single", 44100.0);
            const auto only = library.getSelectedFile();
            library.advanceToRandomClip (44100.0);
            expect (library.getSelectedFile() == only);

            root.deleteRecursively();
        }

        beginTest ("a pack: clips, tags and authors come from pack.json; bad licences are refused (ADR 040)");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("abcTrainPackTest").getNonexistentSibling();
            const auto packDir = root.getChildFile ("rock-basics");
            writeTestWav (packDir.getChildFile ("a.wav"), 44100.0, 4410);
            writeTestWav (packDir.getChildFile ("b.wav"), 44100.0, 4410);
            writeTestWav (packDir.getChildFile ("c.wav"), 44100.0, 4410);
            writeTestWav (packDir.getChildFile ("d.wav"), 44100.0, 4410);
            writeTestWav (root.getChildFile ("outside.wav"), 44100.0, 4410);

            packDir.getChildFile ("pack.json").replaceWithText (juce::String (juce::CharPointer_UTF8 (R"({
              "abcTrainPack": 1, "id": "rock-basics", "version": "1.0.0",
              "title": { "en": "Rock basics", "ru": "Рок: основы" },
              "clips": [
                { "file": "a.wav", "tags": { "genre": ["rock"], "content": "vocal-female", "instruments": ["guitar"] },
                  "source": { "title": "Song A", "author": "Band A", "url": "https://a", "license": "CC-BY-4.0" } },
                { "file": "b.wav", "tags": { "genre": ["rock"], "content": "instrumental" },
                  "source": { "title": "Song B", "author": "Band B", "license": "CC0-1.0" } },
                { "file": "c.wav", "tags": { "genre": ["rock"] },
                  "source": { "title": "Song C", "author": "Band C", "license": "CC-BY-NC-4.0" } },
                { "file": "d.wav", "tags": { "genre": ["rock"] },
                  "source": { "title": "Song D", "license": "CC-BY-4.0" } },
                { "file": "../outside.wav", "source": { "author": "X", "license": "CC0-1.0" } }
              ] })")));

            auto options = makeTempOptions ("pack");
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary library (properties);
            library.setRootFolder (root);

            const ReferenceAudioLibrary::Category* pack = nullptr;
            for (const auto& category : library.getCategories())
                if (category.isPack)
                    pack = &category;

            expect (pack != nullptr);

            if (pack != nullptr)
            {
                expectEquals (pack->titleRu, juce::String (juce::CharPointer_UTF8 ("\xd0\xa0\xd0\xbe\xd0\xba: \xd0\xbe\xd1\x81\xd0\xbd\xd0\xbe\xd0\xb2\xd1\x8b")));
                expectEquals (pack->files.size(), 2);   // NC refused, no author refused, outside refused
                expectEquals (pack->clips.size(), pack->files.size());
                expectEquals (pack->clips.getReference (0).content, juce::String ("vocal-female"));
            }

            expectEquals (library.getRejectedClipCount(), 2);

            const auto female = library.filesMatching ({ "rock", "vocal-female", {} });
            expectEquals (female.size(), 1);
            expectEquals (library.filesMatching ({ "hip-hop", {}, {} }).size(), 0);

            auto sawBandA = false;
            for (const auto& credit : library.getCredits())
                sawBandA = sawBandA || (credit.author == "Band A" && credit.license == "CC-BY-4.0");
            expect (sawBandA);

            root.deleteRecursively();
        }

        beginTest ("the exercise-sound preference persists and defaults to on");
        {
            auto options = makeTempOptions ("prefer");
            {
                juce::PropertiesFile properties (options);
                ReferenceAudioLibrary library (properties);
                expect (library.getPreferExerciseSound());
                library.setPreferExerciseSound (false);
            }
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary reloaded (properties);
            expect (! reloaded.getPreferExerciseSound());
        }

        beginTest ("licences: CC0 / BY / BY-SA / permission in, NC and ND out");
        {
            for (const auto* ok : { "CC0-1.0", "CC-BY-4.0", "CC-BY-SA-4.0", "permission", "PD" })
                expect (ReferenceAudioLibrary::isAllowedLicense (ok), ok);

            for (const auto* no : { "CC-BY-NC-4.0", "CC-BY-ND-4.0", "CC-BY-NC-SA-4.0", "Pixabay", "" })
                expect (! ReferenceAudioLibrary::isAllowedLicense (no), no);
        }
    }

private:
    static void writeClickTrain (const juce::File& destination, double sampleRate,
                                 double seconds, double clicksPerSecond)
    {
        const auto numSamples = (int) (sampleRate * seconds);
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        const auto period = (int) (sampleRate / clicksPerSecond);
        const auto decay = (int) (sampleRate * 0.03);
        juce::Random random (99);

        for (int start = 0; start + decay < numSamples; start += period)
            for (int i = 0; i < decay; ++i)
            {
                const auto envelope = 1.0f - (float) i / (float) decay;
                const auto value = (random.nextFloat() * 2.0f - 1.0f) * envelope * 0.7f;
                buffer.setSample (0, start + i, value);
                buffer.setSample (1, start + i, value);
            }

        destination.deleteFile();

        if (auto stream = destination.createOutputStream())
        {
            juce::WavAudioFormat wav;

            if (auto* writer = wav.createWriterFor (stream.get(), sampleRate, 2, 16, {}, 0))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
                delete writer;
            }
        }
    }
};


static ReferenceAudioLibraryTest referenceAudioLibraryTest;

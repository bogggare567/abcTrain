#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "shared/audio/InstrumentLabel.h"
#include "shared/audio/ReferenceAudioLibrary.h"

// The instrument sorter and what the Training Sounds page now does by hand:
// a fragment cut on the bar grid that loops on its own, deleting a clip,
// installing a pack. The rules are the pack builder's
// (tools/library/prepare_audio.py) - the name tests below are its own
// examples, so a change on one side shows up here.
namespace
{
    constexpr double rate = 44100.0;

    // A kick every beat: a 55 Hz thump with a fast decay.
    juce::AudioBuffer<float> kicks (double bpm, double seconds, int channels = 2)
    {
        juce::AudioBuffer<float> out (channels, (int) (seconds * rate));
        out.clear();
        const auto period = 60.0 / bpm * rate;

        for (double at = 0.0; at + 0.3 * rate < out.getNumSamples(); at += period)
            for (int i = 0; i < (int) (0.3 * rate); ++i)
            {
                const auto t = (double) i / rate;
                const auto v = (float) (0.8 * std::sin (juce::MathConstants<double>::twoPi * 55.0 * t) * std::exp (-t * 14.0));
                for (int ch = 0; ch < channels; ++ch)
                    out.setSample (ch, (int) at + i, v);
            }

        return out;
    }

    // A sustained bass line: one note a bar, a short gap before the next.
    juce::AudioBuffer<float> bassLine (double bpm, double seconds)
    {
        juce::AudioBuffer<float> out (1, (int) (seconds * rate));
        out.clear();
        const double notes[] = { 55.0, 73.4, 65.4, 49.0 };
        const auto bar = 240.0 / bpm;

        for (int k = 0; (k + 1) * bar <= seconds; ++k)
        {
            const auto a = (int) (k * bar * rate), b = (int) (((k + 1) * bar - 0.05) * rate);
            for (int i = a; i < b; ++i)
            {
                const auto t = (double) (i - a) / rate;
                out.setSample (0, i, (float) (0.5 * std::sin (juce::MathConstants<double>::twoPi * notes[k % 4] * t)
                                                  * juce::jmin (1.0, t / 0.01)));
            }
        }

        return out;
    }

    // Broad, dense and wide: noise in both channels, independent, over a
    // kick - the shape of a finished mix.
    juce::AudioBuffer<float> mixLike (double seconds)
    {
        auto out = kicks (120.0, seconds);
        juce::Random random (7);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < out.getNumSamples(); ++i)
                out.addSample (ch, i, (random.nextFloat() * 2.0f - 1.0f) * 0.25f);

        return out;
    }

    InstrumentLabel::Verdict judge (const juce::AudioBuffer<float>& audio, const juce::String& name, bool songShaped = false)
    {
        return InstrumentLabel::decide (InstrumentLabel::fromName (name),
                                        InstrumentLabel::measure (audio, 0, audio.getNumSamples(), rate),
                                        songShaped);
    }

    juce::File writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        juce::WavAudioFormat wav;
        auto stream = file.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (stream.get(), rate, (unsigned int) audio.getNumChannels(), 16, {}, 0));

        if (writer != nullptr)
        {
            stream.release();
            writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
        }

        return file;
    }

    juce::AudioBuffer<float> readWav (const juce::File& file, double& fileRate)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        juce::AudioBuffer<float> audio;
        if (reader == nullptr)
            return audio;

        fileRate = reader->sampleRate;
        audio.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&audio, 0, audio.getNumSamples(), 0, true, true);
        return audio;
    }

    juce::PropertiesFile::Options tempOptions (const juce::String& suffix)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "EarTrainerTests";
        options.filenameSuffix = "settings";
        options.folderName = "EarTrainerTests_instrument_" + suffix;
        options.getDefaultFile().deleteFile();
        return options;
    }
}

class InstrumentLabelTest : public juce::UnitTest
{
public:
    InstrumentLabelTest() : juce::UnitTest ("InstrumentLabel", "Audio") {}

    void runTest() override
    {
        using I = InstrumentLabel::Instrument;

        beginTest ("names say the instrument in English and Russian, whole words only");
        {
            const std::pair<const char*, I> named[] = {
                { "kick in 0001 [2026-07-06 195059]", I::kick }, { "Snare Top", I::snare }, { "OH L", I::cymbals },
                { "Tom2", I::toms }, { "Bass Drum", I::kick }, { "hi-hat close", I::hihat },
                { "Leorint 14-Vital", I::keys }, { "guitar 0001", I::guitar }, { "pipe 0001", I::wind },
                { "min bbm mix", I::mix },
            };

            for (const auto& [name, instrument] : named)
            {
                const auto hit = InstrumentLabel::fromName (name);
                expect (hit.has_value() && hit->instrument == instrument,
                        juce::String (name) + " -> " + (hit ? InstrumentLabel::idOf (hit->instrument) : "nothing"));
            }

            const std::pair<juce::String, I> cyrillic[] = {
                { juce::String::fromUTF8 ("БОЧКА"), I::kick },            // upper case, whatever the locale
                { juce::String::fromUTF8 ("бас-гитара"), I::bass },
                { juce::String::fromUTF8 ("Вокал Ваня"), I::vocal },
                { juce::String::fromUTF8 ("дудка"), I::wind },
                { juce::String::fromUTF8 ("малый барабан"), I::snare },
            };

            for (const auto& [name, instrument] : cyrillic)
            {
                const auto hit = InstrumentLabel::fromName (name);
                expect (hit.has_value() && hit->instrument == instrument, name);
            }

            // Names that say nothing must say nothing - then the sound decides.
            for (const auto* nothing : { "vanya 0001", "Audio 3", "Leorint 26-Audio 3", "subject", "sonata", "banana" })
                expect (! InstrumentLabel::fromName (nothing).has_value(), nothing);

            expect (! InstrumentLabel::fromName (juce::String::fromUTF8 ("Файл 3")).has_value());
        }

        beginTest ("with no name, the sound names what it can: kick, bass, a song-length mix");
        {
            const auto kick = judge (kicks (120.0, 10.0, 1), "Audio 3");
            expect (kick.instrument == I::kick, kick.reason);

            const auto bass = judge (bassLine (120.0, 10.0), "Audio 4");
            expect (bass.instrument == I::bass, bass.reason);

            const auto mix = judge (mixLike (10.0), "Track 1", true);
            expect (mix.instrument == I::mix, mix.reason);
        }

        beginTest ("unsure goes to Other; a name the sound plainly contradicts goes to Other");
        {
            // A plain tone is melodic, and melodic instruments are never
            // named by sound alone.
            juce::AudioBuffer<float> tone (1, (int) (8.0 * rate));
            for (int i = 0; i < tone.getNumSamples(); ++i)
                tone.setSample (0, i, 0.4f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * (float) i / (float) rate));

            const auto unnamed = judge (tone, "take 5");
            expect (unnamed.instrument == I::other, unnamed.reason);

            const auto quarrel = judge (kicks (120.0, 10.0, 1), "guitar DI");
            expect (quarrel.instrument == I::other, quarrel.reason);

            // ...but a name the sound merely fails to confirm stands.
            const auto guitar = judge (tone, "guitar DI");
            expect (guitar.instrument == I::guitar, guitar.reason);
        }

        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("abcTrainInstrumentTest").getNonexistentSibling();
        root.createDirectory();

        auto options = tempOptions ("lib");
        juce::PropertiesFile properties (options);
        ReferenceAudioLibrary library (properties);
        library.setRootFolder (root.getChildFile ("library"));
        root.getChildFile ("library").createDirectory();

        beginTest ("a hand-picked fragment moves onto the grid and loops without a jump");
        {
            // Twenty seconds of kicks at 120 BPM, outside the library.
            const auto source = writeWav (root.getChildFile ("my kick take.wav"), kicks (120.0, 20.0));

            // A sloppy selection: 3.13 s to 10.9 s - neither on a beat nor
            // a whole number of bars (7.77 s is 3.9 bars of 2 s).
            const auto fragment = library.saveFragment (source, 3.13, 10.9);

            expect (fragment.file.existsAsFile(), "nothing was written");
            expect (fragment.onBeatGrid, "the grid was not found on a metronomic kick");
            expectWithinAbsoluteError (fragment.bpm, 120.0, 1.0);
            expectEquals (fragment.bars, 4);
            expectWithinAbsoluteError (fragment.seconds, 8.0, 0.05);
            expectEquals (fragment.folderName, juce::String ("Kick"));

            double fileRate = 0.0;
            const auto clip = readWav (fragment.file, fileRate);
            expectWithinAbsoluteError (clip.getNumSamples() / fileRate, 8.0, 0.05);

            // The seam: last sample to first is no bigger a step than the
            // largest step inside the clip (16-bit rounding aside).
            auto largestStep = 0.0f;
            for (int i = 1; i < clip.getNumSamples(); ++i)
                largestStep = juce::jmax (largestStep, std::abs (clip.getSample (0, i) - clip.getSample (0, i - 1)));

            const auto seamStep = std::abs (clip.getSample (0, 0) - clip.getSample (0, clip.getNumSamples() - 1));
            expect (seamStep <= largestStep + 1.0e-3f,
                    "the loop jumps at the seam: " + juce::String (seamStep) + " vs " + juce::String (largestStep));
        }

        beginTest ("a fragment of a clip already in the library goes next to it");
        {
            const auto inside = writeWav (root.getChildFile ("library/Bass/line.wav"), bassLine (120.0, 16.0));
            const auto fragment = library.saveFragment (inside, 0.0, 8.0);
            expect (fragment.file.getParentDirectory() == inside.getParentDirectory());
        }

        beginTest ("deleting a clip: only inside the library, and an emptied folder goes too");
        {
            const auto lonely = writeWav (root.getChildFile ("library/Snare/one.wav"), kicks (100.0, 4.0));
            expect (library.canDelete (lonely));
            expect (library.deleteClip (lonely));
            expect (! lonely.existsAsFile(), "the clip is still there");
            expect (! root.getChildFile ("library/Snare").exists(), "an empty folder was left behind");

            const auto outside = writeWav (root.getChildFile ("not in the library.wav"), kicks (100.0, 4.0));
            expect (! library.canDelete (outside));
            expect (! library.deleteClip (outside));
            expect (outside.existsAsFile(), "a file outside the library was touched");
        }

        beginTest ("a pack sorted into instrument folders shows one category per folder; a zip installs");
        {
            const auto packSource = root.getChildFile ("build/bogdan-own");
            writeWav (packSource.getChildFile ("kick/a.wav"), kicks (120.0, 4.0));
            writeWav (packSource.getChildFile ("mix/b.wav"), mixLike (4.0));

            const auto clip = [] (const char* file)
            {
                return juce::String ("{\"file\":\"") + file + "\",\"source\":{\"author\":\"Test Author\",\"license\":\"CC-BY-4.0\"}}";
            };

            packSource.getChildFile ("pack.json").replaceWithText (
                "{\"abcTrainPack\":1,\"id\":\"bogdan-own\",\"title\":{\"en\":\"Own\",\"ru\":\"Own\"},\"clips\":["
                + clip ("mix/b.wav") + "," + clip ("kick/a.wav") + "]}");

            const auto zipFile = root.getChildFile ("bogdan-own.zip");
            {
                juce::ZipFile::Builder builder;
                for (const auto& f : packSource.findChildFiles (juce::File::findFiles, true))
                    builder.addFile (f, 0, "bogdan-own/" + f.getRelativePathFrom (packSource).replaceCharacter ('\\', '/'));

                juce::FileOutputStream out (zipFile);
                builder.writeToStream (out, nullptr);
            }

            expect (ReferenceAudioLibrary::looksLikePack (zipFile));
            expect (! ReferenceAudioLibrary::looksLikePack (root.getChildFile ("my kick take.wav")));
            expectEquals (library.installPack (zipFile), juce::String());

            library.rescan();
            juce::StringArray names;
            for (const auto& c : library.getCategories())
                if (c.isPack)
                    names.add (c.name + ":" + c.packPart);

            // Kick before mix: instruments in their usual order, whatever
            // the manifest's order.
            expectEquals (names.joinIntoString ("|"), juce::String ("bogdan-own/kick:kick|bogdan-own/mix:mix"));

            // Installing again replaces rather than duplicates.
            expectEquals (library.installPack (zipFile), juce::String());
            library.rescan();
            auto packParts = 0;
            for (const auto& c : library.getCategories())
                packParts += c.isPack ? 1 : 0;
            expectEquals (packParts, 2);
        }

        root.deleteRecursively();
    }
};

static InstrumentLabelTest instrumentLabelTest;

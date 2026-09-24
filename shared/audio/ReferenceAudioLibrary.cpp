#include "shared/audio/ReferenceAudioLibrary.h"
#include "shared/audio/AudioSliceAnalyzer.h"
#include "shared/audio/InstrumentLabel.h"
#include "SampleBinaryData.h"
#include <array>
#include <map>

namespace
{
    constexpr const char* rootFolderKey = "referenceAudio.rootFolder";
    constexpr const char* selectedFileKey = "referenceAudio.selectedFile";

    juce::File defaultRootFolder()
    {
        // The app's own storage, not the music folder.
        //
        // Imported clips are *derived* files - eight-second cuts the app
        // made and manages - and putting them in someone's music library
        // means littering it with hundreds of them. It also made "where do
        // I point this" a question the player had to answer before they
        // could find out whether the feature was worth anything.
        return ReferenceAudioLibrary::getManagedLibraryFolder();
    }

    // Reads a file for import. False when it is not audio or cannot be
    // read, which the importers treat as "skip it and carry on".
    bool decodeForImport (juce::AudioFormatManager& formats, const juce::File& file,
                          juce::AudioBuffer<float>& audio, double& sampleRate,
                          double maxMinutes = 20.0)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr || reader->lengthInSamples <= 0)
            return false;

        // Guard against a file so long it would not fit in memory. Twenty
        // minutes is more than any reasonable source and well inside what
        // a float buffer can hold.
        const auto maxSamples = (juce::int64) (reader->sampleRate * maxMinutes * 60.0);
        const auto length = (int) juce::jmin (reader->lengthInSamples, maxSamples);

        audio.setSize ((int) juce::jmax (1u, reader->numChannels), length);

        if (! reader->read (&audio, 0, length, 0, true, true))
            return false;

        sampleRate = reader->sampleRate;
        return true;
    }

    // Crossfade at the seam. On the bar grid the loop's end and start are
    // the same point of the music, so a few milliseconds hides the join;
    // off the grid they are two different moments, and it takes a longer
    // blend to stop the jump from being heard as one.
    constexpr double gridCrossfadeSeconds = 0.015;
    constexpr double freeCrossfadeSeconds = 0.12;

    // Writes audio[start, start + length) as a 16-bit WAV that loops on
    // its own.
    //
    // The seam is closed by folding what came *after* the cut into its
    // first moments (equal-power): when a player wraps from the last
    // sample back to the first, the first is now what followed the last in
    // the source, so there is no jump to click on. The earlier version
    // faded both ends to silence, which removes the click and puts a hole
    // in the groove on every repeat instead - a dip the ear locks onto as
    // surely as a click. Where the source ends right at the cut, there is
    // nothing to fold in and short fades remain the fallback.
    bool writeLoopFile (const juce::AudioBuffer<float>& audio, int start, int length, int crossfade,
                        double sampleRate, const juce::File& destination)
    {
        if (length <= 0 || start < 0 || start + length > audio.getNumSamples())
            return false;

        std::unique_ptr<juce::FileOutputStream> stream (destination.createOutputStream());

        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            // 16-bit, not 24: these are training loops, not masters.
            // The exercises hide changes of a decibel or more in them,
            // and 24-bit buys nothing against that while costing half
            // as much disk again.
            wav.createWriterFor (stream.get(), sampleRate,
                                  (unsigned int) audio.getNumChannels(), 16, {}, 0));

        if (writer == nullptr)
            return false;

        stream.release();   // the writer owns it now

        juce::AudioBuffer<float> clip (audio.getNumChannels(), length);

        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            clip.copyFrom (channel, 0, audio, channel, start, length);

        const auto fold = juce::jmin (crossfade, length / 4, audio.getNumSamples() - (start + length));

        if (fold > 8)
        {
            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            {
                auto* head = clip.getWritePointer (channel);
                const auto* tail = audio.getReadPointer (channel, start + length);

                for (int i = 0; i < fold; ++i)
                {
                    const auto phase = ((float) i + 0.5f) / (float) fold * juce::MathConstants<float>::halfPi;
                    head[i] = head[i] * std::sin (phase) + tail[i] * std::cos (phase);
                }
            }
        }
        else
        {
            const auto fadeSamples = juce::jmin (length / 8, (int) (sampleRate * 0.01));
            clip.applyGainRamp (0, fadeSamples, 0.0f, 1.0f);
            clip.applyGainRamp (length - fadeSamples, fadeSamples, 1.0f, 0.0f);
        }

        return writer->writeFromAudioSampleBuffer (clip, 0, clip.getNumSamples());
    }

    int crossfadeFor (bool onBeatGrid, double sampleRate)
    {
        return (int) ((onBeatGrid ? gridCrossfadeSeconds : freeCrossfadeSeconds) * sampleRate);
    }

    // What instrument a whole source file is: its name (or its folder's),
    // checked against the median of how its slices sound.
    InstrumentLabel::Verdict labelFor (const juce::File& file, const juce::AudioBuffer<float>& audio,
                                       double sampleRate, const std::vector<std::pair<int, int>>& ranges,
                                       double sourceSeconds)
    {
        std::vector<InstrumentLabel::Features> measured;

        for (const auto& [start, length] : ranges)
            measured.push_back (InstrumentLabel::measure (audio, start, length, sampleRate));

        auto name = InstrumentLabel::fromName (file.getFileNameWithoutExtension());

        if (! name.has_value())
            name = InstrumentLabel::fromName (file.getParentDirectory().getFileName());

        const auto songShaped = sourceSeconds >= 60.0 && sourceSeconds <= 1200.0;
        return InstrumentLabel::decide (name, InstrumentLabel::median (measured), songShaped);
    }

    // Writes each slice of `audio` as a loop in `folder`, named after the
    // source. Returns how many were written.
    int writeClips (const juce::AudioBuffer<float>& audio,
                    double sampleRate,
                    const std::vector<AudioSliceAnalyzer::Slice>& slices,
                    const juce::String& baseName,
                    const juce::File& folder,
                    const std::function<bool()>& shouldStop)
    {
        auto written = 0;

        if (! folder.createDirectory())
            return 0;

        for (size_t i = 0; i < slices.size(); ++i)
        {
            if (shouldStop != nullptr && shouldStop())
                break;

            const auto& slice = slices[i];

            const auto destination = folder.getChildFile (
                baseName + " " + juce::String ((int) i + 1) + ".wav")
                    .getNonexistentSibling();

            if (writeLoopFile (audio, slice.startSample, slice.numSamples,
                               crossfadeFor (slice.onBeatGrid, sampleRate), sampleRate, destination))
                ++written;
        }

        return written;
    }

    bool holdsAudio (const juce::File& folder)
    {
        return ! folder.findChildFiles (juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg").isEmpty();
    }
}

juce::PropertiesFile::Options ReferenceAudioLibrary::makeDefaultOptions()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "abcTrain";
    options.filenameSuffix = "referenceaudio";
    options.folderName = "abcTrain";
    options.osxLibrarySubFolder = "Application Support";
    return options;
}

ReferenceAudioLibrary::ReferenceAudioLibrary (juce::PropertiesFile& propertiesFileToUse)
    : properties (propertiesFileToUse)
{
    formatManager.registerBasicFormats();

    const auto savedRoot = properties.getValue (rootFolderKey);
    rootFolder = savedRoot.isNotEmpty() ? juce::File (savedRoot) : defaultRootFolder();

    // Migrate off the old default.
    //
    // Until v1.1 the default root was <Music>/ABCTrain. That was never a
    // folder anybody *chose* - it is where the app happened to look - and
    // leaving an updating player pointed at it means an empty library with
    // no explanation, immediately after the feature that fills it was
    // added. Only the untouched default moves: a folder the player
    // actually picked, or one with anything in it, is left exactly where
    // it is.
    {
        const auto legacyDefault = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                       .getChildFile ("ABCTrain");

        if (rootFolder == legacyDefault
            && legacyDefault.findChildFiles (juce::File::findDirectories, false).isEmpty())
        {
            rootFolder = defaultRootFolder();
            properties.setValue (rootFolderKey, rootFolder.getFullPathName());
            properties.saveIfNeeded();
        }
    }

    rescan();

    pinned = properties.getBoolValue ("referencePinned", false);

    const auto savedSelection = properties.getValue (selectedFileKey);
    if (savedSelection.isNotEmpty())
        selectedFile = juce::File (savedSelection);
    // The buffer itself is loaded lazily by prepare(sampleRate) once the
    // real processing sample rate is known - see its comment.
}

void ReferenceAudioLibrary::setRootFolder (const juce::File& newRoot)
{
    rootFolder = newRoot;
    properties.setValue (rootFolderKey, newRoot.getFullPathName());
    properties.saveIfNeeded();
    rescan();
}

void ReferenceAudioLibrary::addBuiltInCategories()
{
    struct BuiltInFile
    {
        const char* categoryName;
        const char* fileName;
        const char* data;
        int size;
    };

    // Every one of these is a short, programmatically-synthesized tone
    // (see assets/samples/ and decisions/018) - never a recording of, or
    // extracted from, anyone else's copyrighted material.
    static const std::array<BuiltInFile, 5> builtIns {{
        { "Built-in Percussive", "Kick.wav",  SampleBinaryData::Kick_wav,  SampleBinaryData::Kick_wavSize },
        { "Built-in Percussive", "Snare.wav", SampleBinaryData::Snare_wav, SampleBinaryData::Snare_wavSize },
        { "Built-in Sustained",  "Pad.wav",   SampleBinaryData::Pad_wav,   SampleBinaryData::Pad_wavSize },
        { "Built-in Sustained",  "Pluck.wav", SampleBinaryData::Pluck_wav, SampleBinaryData::Pluck_wavSize },
        { "Built-in Sustained",  "Tone.wav",  SampleBinaryData::Tone_wav,  SampleBinaryData::Tone_wavSize },
    }};

    const auto cacheDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("abcTrain")
                               .getChildFile ("BuiltInSamples");
    cacheDir.createDirectory();

    juce::Array<Category> builtInCategories;

    for (const auto& b : builtIns)
    {
        auto file = cacheDir.getChildFile (b.fileName);
        if (! file.existsAsFile() || file.getSize() != (juce::int64) b.size)
            file.replaceWithData (b.data, (size_t) b.size);

        const juce::String categoryName (b.categoryName);
        Category* target = nullptr;
        for (auto& c : builtInCategories)
            if (c.name == categoryName)
            {
                target = &c;
                break;
            }

        if (target == nullptr)
        {
            builtInCategories.add ({ categoryName, {} });
            target = &builtInCategories.getReference (builtInCategories.size() - 1);
        }

        target->files.add (file);

        // Synthesized by this project, public domain. Credited all the
        // same, so the credits page accounts for every sound it can play.
        ClipInfo info;
        info.credit = { juce::String (b.fileName).upToLastOccurrenceOf (".", false, false),
                        "abcTrain (synthesized)", "https://github.com/bogggare567/abcTrain", "CC0-1.0" };
        target->clips.add (info);
    }

    // Inserted ahead of anything scanned from rootFolder, so built-in
    // categories always land at the front - TrainingSoundsComponent's
    // lock rule is "index < maxLevelReached", and level starts at 1, so
    // the first built-in category is always unlocked.
    for (int i = builtInCategories.size() - 1; i >= 0; --i)
        categories.insert (0, builtInCategories.getReference (i));
}

void ReferenceAudioLibrary::rescan()
{
    categories.clear();
    rejectedClips = 0;
    addBuiltInCategories();

    if (! rootFolder.isDirectory())
        return;

    for (const auto& subDir : rootFolder.findChildFiles (juce::File::findDirectories, false))
    {
        if (subDir.getFileName().startsWithChar ('.'))
            continue;   // an install in progress, or the OS's own


        // A folder with a pack.json is a pack: its clips, tags and authors
        // come from the manifest. Anything else is a folder somebody made,
        // and every playable file in it is a clip with nothing known about
        // it - exactly as before packs existed.
        const auto manifest = subDir.getChildFile ("pack.json");

        if (manifest.existsAsFile())
        {
            auto pack = readPack (subDir, manifest);

            if (pack.files.isEmpty())
                continue;

            // A pack sorted into subfolders (prepare_audio.py writes one per
            // instrument) is shown as one category per subfolder: two
            // hundred clips in one list is not a library, it is a pile.
            // Clips at the pack's top level stay together under its name.
            std::map<juce::String, Category> parts;
            juce::StringArray order;

            for (int i = 0; i < pack.files.size(); ++i)
            {
                const auto relative = pack.files[i].getRelativePathFrom (subDir).replaceCharacter ('\\', '/');
                const auto sub = relative.containsChar ('/') ? relative.upToFirstOccurrenceOf ("/", false, false)
                                                             : juce::String();
                auto [it, fresh] = parts.try_emplace (sub);
                auto& part = it->second;

                if (fresh)
                {
                    part = pack;
                    part.files.clear();
                    part.clips.clear();
                    part.name = sub.isEmpty() ? pack.name : pack.name + "/" + sub;
                    part.packPart = sub;
                    order.add (sub);
                }

                part.files.add (pack.files[i]);
                part.clips.add (pack.clips[i]);
            }

            // Instruments in their usual order (kick first, other last),
            // anything else after them alphabetically.
            std::sort (order.begin(), order.end(), [] (const juce::String& a, const juce::String& b)
            {
                const auto rank = [] (const juce::String& id)
                {
                    const auto i = InstrumentLabel::fromId (id);
                    return i.has_value() ? (int) *i : InstrumentLabel::numInstruments;
                };
                return rank (a) != rank (b) ? rank (a) < rank (b) : a < b;
            });

            for (const auto& sub : order)
                categories.add (std::move (parts[sub]));

            continue;
        }

        Category category;
        category.name = subDir.getFileName();

        for (const auto& file : subDir.findChildFiles (juce::File::findFiles, false))
        {
            // Actually asking each format to open the file (rather than
            // guessing from its extension) is the only way to be sure
            // it's really playable audio, and this only runs when the
            // user changes/opens the training-sounds folder - not
            // remotely audio-thread-hot.
            std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
            if (reader != nullptr)
            {
                category.files.add (file);
                category.clips.add ({});
            }
        }

        if (! category.files.isEmpty())
            categories.add (category);
    }
}

bool ReferenceAudioLibrary::isAllowedLicense (const juce::String& license)
{
    // The rule from docs/design/sound-library.md, enforced where the audio
    // is read as well as where packs are built: public domain, CC0, CC BY
    // and CC BY-SA, or the author's written permission. No NC (closes any
    // paid future and is arguable even with donations), no ND (a clip is
    // an adaptation). A clip that fails this is not offered.
    static const juce::StringArray allowed {
        "CC0-1.0", "PD", "CC-BY-3.0", "CC-BY-4.0", "CC-BY-SA-3.0", "CC-BY-SA-4.0", "permission"
    };

    return allowed.contains (license.trim(), true);
}

ReferenceAudioLibrary::Category ReferenceAudioLibrary::readPack (const juce::File& folder,
                                                                  const juce::File& manifest)
{
    Category pack;
    pack.name = folder.getFileName();
    pack.isPack = true;

    const auto json = juce::JSON::parse (manifest);

    if (! json.isObject() || (int) json.getProperty ("abcTrainPack", 0) != 1)
        return pack;   // not a pack this build understands - offer nothing

    pack.packId = json.getProperty ("id", pack.name).toString();
    pack.packVersion = json.getProperty ("version", "").toString();

    if (const auto title = json.getProperty ("title", {}); title.isObject())
    {
        pack.titleEn = title.getProperty ("en", pack.name).toString();
        pack.titleRu = title.getProperty ("ru", pack.titleEn).toString();
    }
    else
    {
        pack.titleEn = pack.titleRu = json.getProperty ("title", pack.name).toString();
    }

    const auto toStrings = [] (const juce::var& v)
    {
        juce::StringArray out;
        if (const auto* array = v.getArray())
            for (const auto& item : *array)
                out.add (item.toString());
        return out;
    };

    if (const auto* clips = json.getProperty ("clips", {}).getArray())
    {
        for (const auto& clip : *clips)
        {
            const auto file = folder.getChildFile (clip.getProperty ("file", "").toString());

            // A manifest may not reach outside its own folder.
            if (! file.existsAsFile() || ! file.isAChildOf (folder))
                continue;

            ClipInfo info;
            const auto tags = clip.getProperty ("tags", {});
            info.genres = toStrings (tags.getProperty ("genre", {}));
            info.instruments = toStrings (tags.getProperty ("instruments", {}));
            info.content = tags.getProperty ("content", "").toString();
            info.character = tags.getProperty ("character", "").toString();

            const auto source = clip.getProperty ("source", {});
            info.credit.title = source.getProperty ("title", "").toString();
            info.credit.author = source.getProperty ("author", "").toString();
            info.credit.url = source.getProperty ("url", "").toString();
            info.credit.license = source.getProperty ("license", "").toString();

            // No author or no acceptable licence: not offered. A CC BY clip
            // without its author cannot be credited, which is the one thing
            // its licence asks for.
            if (info.credit.author.isEmpty() || ! isAllowedLicense (info.credit.license))
            {
                ++rejectedClips;
                continue;
            }

            pack.files.add (file);
            pack.clips.add (info);
        }
    }

    return pack;
}

juce::Array<ReferenceAudioLibrary::Credit> ReferenceAudioLibrary::getCredits() const
{
    juce::Array<Credit> credits;

    for (const auto& category : categories)
        for (const auto& clip : category.clips)
        {
            if (clip.credit.author.isEmpty())
                continue;

            auto seen = false;
            for (const auto& c : credits)
                seen = seen || (c.title == clip.credit.title && c.author == clip.credit.author);

            if (! seen)
                credits.add (clip.credit);
        }

    return credits;
}

juce::Array<juce::File> ReferenceAudioLibrary::filesMatching (const Filter& filter) const
{
    juce::Array<juce::File> matches;

    for (const auto& category : categories)
        for (int i = 0; i < category.files.size(); ++i)
        {
            const auto& clip = category.clips[i];

            if (filter.genre.isNotEmpty() && ! clip.genres.contains (filter.genre, true))
                continue;
            if (filter.content.isNotEmpty() && clip.content != filter.content)
                continue;
            if (filter.instrument.isNotEmpty() && ! clip.instruments.contains (filter.instrument, true))
                continue;

            matches.add (category.files[i]);
        }

    return matches;
}

void ReferenceAudioLibrary::setPreferExerciseSound (bool shouldPrefer)
{
    properties.setValue ("referencePreferExerciseSound", shouldPrefer);
    properties.saveIfNeeded();
}

bool ReferenceAudioLibrary::getPreferExerciseSound() const
{
    return properties.getBoolValue ("referencePreferExerciseSound", true);
}

bool ReferenceAudioLibrary::selectFile (const juce::File& file, double targetSampleRate)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;

    const auto sourceSampleRate = reader->sampleRate;
    const auto sourceLength = (int) juce::jmin ((juce::int64) (maxBufferSeconds * sourceSampleRate),
                                                 reader->lengthInSamples);
    if (sourceLength <= 0 || reader->numChannels == 0)
        return false;

    juce::AudioBuffer<float> sourceBuffer ((int) reader->numChannels, sourceLength);
    reader->read (&sourceBuffer, 0, sourceLength, 0, true, true);

    // Downmix to mono - every game applies the same value to every output
    // channel anyway (see e.g. EQGame::process()), so there's no benefit
    // to keeping more than one channel of the reference signal around.
    juce::AudioBuffer<float> mono (1, sourceLength);
    mono.clear();
    for (int ch = 0; ch < sourceBuffer.getNumChannels(); ++ch)
        mono.addFrom (0, 0, sourceBuffer, ch, 0, sourceLength, 1.0f / (float) sourceBuffer.getNumChannels());

    std::unique_ptr<juce::AudioBuffer<float>> resampled;

    if (targetSampleRate > 0.0 && ! juce::approximatelyEqual (sourceSampleRate, targetSampleRate))
    {
        const auto ratio = sourceSampleRate / targetSampleRate;
        const auto targetLength = juce::jmax (1, (int) ((double) sourceLength / ratio));
        resampled = std::make_unique<juce::AudioBuffer<float>> (1, targetLength);

        juce::LagrangeInterpolator interpolator;
        interpolator.reset();
        interpolator.process (ratio, mono.getReadPointer (0), resampled->getWritePointer (0), targetLength);
    }
    else
    {
        resampled = std::make_unique<juce::AudioBuffer<float>> (mono);
    }

    auto* stored = loadedBuffers.add (resampled.release());
    activeBuffer.store (stored);

    // Keep a short tail of previous clips rather than every clip ever
    // loaded. Freeing the outgoing buffer immediately would be a
    // use-after-free - the audio thread loads the pointer once and then
    // reads through the rest of its block - so the old code never freed
    // anything at all. That is correct and unbounded: EarTrainer selects a
    // fresh clip on *every round*, at up to 20 seconds each, so a long
    // session walked into hundreds of megabytes and then into swap, which
    // is felt as the whole machine stuttering rather than as this plugin.
    //
    // Four is far more than the one block of slack the race actually
    // needs, and it caps the store at a few tens of megabytes.
    while (loadedBuffers.size() > maxRetainedBuffers)
    {
        jassert (loadedBuffers.getFirst() != activeBuffer.load());
        loadedBuffers.remove (0);
    }

    selectedFile = file;
    properties.setValue (selectedFileKey, file.getFullPathName());
    properties.saveIfNeeded();

    return true;
}

void ReferenceAudioLibrary::clearSelection()
{
    activeCategory = {};
    properties.setValue ("referenceCategory", juce::String());

    activeBuffer.store (nullptr);
    selectedFile = juce::File();
    properties.removeValue (selectedFileKey);
    properties.saveIfNeeded();
}

void ReferenceAudioLibrary::prepare (double sampleRate)
{
    if (selectedFile.existsAsFile())
        selectFile (selectedFile, sampleRate);
}


int ReferenceAudioLibrary::importAndSlice (const juce::File& source)
{
    if (! source.exists())
        return 0;

    juce::Array<juce::File> sources;

    if (source.isDirectory())
        sources = source.findChildFiles (juce::File::findFiles, false, "*.wav;*.aiff;*.aif;*.flac;*.mp3");
    else
        sources.add (source);

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    auto written = 0;

    for (const auto& file : sources)
    {
        juce::AudioBuffer<float> audio;
        double sampleRate = 0.0;

        if (! decodeForImport (formats, file, audio, sampleRate))
            continue;   // not audio, or unreadable - skip it and carry on

        const auto slices = AudioSliceAnalyzer::analyse (audio, sampleRate);

        if (slices.empty())
            continue;

        // One folder per source file: its instrument, decided once from the
        // name and all of its slices together. Sorting slice by slice put
        // the verse of a song in one folder and its chorus in another.
        std::vector<std::pair<int, int>> ranges;
        for (const auto& slice : slices)
            ranges.emplace_back (slice.startSample, slice.numSamples);

        const auto verdict = labelFor (file, audio, sampleRate, ranges,
                                       (double) audio.getNumSamples() / sampleRate);

        written += writeClips (audio, sampleRate, slices, file.getFileNameWithoutExtension(),
                               rootFolder.getChildFile (InstrumentLabel::folderNameFor (verdict.instrument)),
                               nullptr);
    }

    return written;
}

juce::File ReferenceAudioLibrary::getManagedLibraryFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("abcTrain")
               .getChildFile ("Training Sounds");
}

int ReferenceAudioLibrary::importAndSliceMany (const juce::Array<juce::File>& sources,
                                                std::function<void (float, juce::String)> onProgress,
                                                std::function<bool()> shouldStop)
{
    auto written = 0;

    for (int i = 0; i < sources.size(); ++i)
    {
        if (shouldStop != nullptr && shouldStop())
            break;

        if (onProgress != nullptr)
            onProgress ((float) i / (float) juce::jmax (1, sources.size()),
                         sources[i].getFileName());

        // A pack is installed, not sliced: its clips are already cut,
        // tagged and credited.
        if (looksLikePack (sources[i]))
        {
            if (installPack (sources[i]).isEmpty())
                written += lastInstalledPack.findChildFiles (juce::File::findFiles, true,
                                                             "*.flac;*.wav;*.aif;*.aiff;*.ogg;*.mp3").size();
            continue;
        }

        written += importAndSlice (sources[i]);
    }

    if (onProgress != nullptr)
        onProgress (1.0f, {});

    return written;
}

void ReferenceAudioLibrary::pinFile (const juce::File& file, double sampleRate)
{
    if (! selectFile (file, sampleRate))
        return;

    pinned = true;
    properties.setValue ("referencePinned", true);
    properties.saveIfNeeded();
}

void ReferenceAudioLibrary::unpinFile()
{
    pinned = false;
    properties.setValue ("referencePinned", false);
    properties.saveIfNeeded();
}

void ReferenceAudioLibrary::setActiveCategory (const juce::String& categoryName, double sampleRate)
{
    // Choosing a category is choosing rotation again.
    pinned = false;
    properties.setValue ("referencePinned", false);

    activeCategory = categoryName;
    properties.setValue ("referenceCategory", activeCategory);
    properties.saveIfNeeded();

    advanceToRandomClip (sampleRate);
}

void ReferenceAudioLibrary::advanceToRandomClip (double sampleRate)
{
    // A pinned clip is a clip the player asked for by name. Rotating away
    // from it on the next round would be the app overruling them.
    if (pinned || activeCategory.isEmpty())
        return;

    for (const auto& category : categories)
    {
        if (category.name != activeCategory)
            continue;

        if (category.files.isEmpty())
            return;

        if (category.files.size() == 1)
        {
            selectFile (category.files.getReference (0), sampleRate);
            return;
        }

        // Avoid playing the same clip twice running. With only a handful
        // of clips a uniform draw repeats often enough to be noticed, and
        // "it played the same thing again" reads as the app being stuck.
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            const auto& candidate = category.files.getReference (random.nextInt (category.files.size()));

            if (candidate != selectedFile)
            {
                selectFile (candidate, sampleRate);
                return;
            }
        }

        return;
    }
}

// ---- packs, fragments, deleting ---------------------------------------------

bool ReferenceAudioLibrary::looksLikePack (const juce::File& file)
{
    if (file.isDirectory())
        return file.getChildFile ("pack.json").existsAsFile();

    if (! file.hasFileExtension ("zip"))
        return false;

    juce::ZipFile zip (file);

    for (int i = 0; i < zip.getNumEntries(); ++i)
        if (const auto* entry = zip.getEntry (i); entry != nullptr
                && (entry->filename == "pack.json" || entry->filename.endsWith ("/pack.json")))
            return true;

    return false;
}

juce::String ReferenceAudioLibrary::installPack (const juce::File& source)
{
    lastInstalledPack = juce::File();

    if (! rootFolder.createDirectory())
        return "cannot create the library folder";

    juce::File packFolder;

    // Unpacked beside the library, then copied in, then removed - whatever
    // happens in between. A dot folder: rescan() skips those.
    struct Scratch
    {
        juce::File dir;
        ~Scratch() { if (dir != juce::File()) dir.deleteRecursively(); }
    } unpacked;

    if (source.isDirectory())
    {
        packFolder = source;
    }
    else
    {
        juce::ZipFile zip (source);
        unpacked.dir = rootFolder.getChildFile (".installing").getNonexistentSibling();
        const auto target = unpacked.dir;
        target.createDirectory();

        if (zip.uncompressTo (target, true).failed())
            return "the zip could not be unpacked";

        // pack.json at the top of the zip, or one folder down - both are
        // how people zip a folder.
        if (target.getChildFile ("pack.json").existsAsFile())
            packFolder = target;
        else
            for (const auto& sub : target.findChildFiles (juce::File::findDirectories, false))
                if (sub.getChildFile ("pack.json").existsAsFile())
                    packFolder = sub;
    }

    if (! packFolder.isDirectory() || ! packFolder.getChildFile ("pack.json").existsAsFile())
        return "no pack.json inside";

    const auto json = juce::JSON::parse (packFolder.getChildFile ("pack.json"));

    if (! json.isObject() || (int) json.getProperty ("abcTrainPack", 0) != 1)
        return "pack.json is not an abcTrain pack";

    // The folder name: the pack's id, so a newer version lands on top of
    // the older one instead of beside it.
    auto name = juce::File::createLegalFileName (json.getProperty ("id", "").toString());
    if (name.isEmpty())
        name = source.getFileNameWithoutExtension();

    const auto destination = rootFolder.getChildFile (name);

    if (destination == packFolder)
    {
        lastInstalledPack = destination;
        return {};
    }

    if (destination.exists())
        destination.deleteRecursively();

    if (! packFolder.copyDirectoryTo (destination))
        return "copying the pack failed";

    lastInstalledPack = destination;
    return {};
}

bool ReferenceAudioLibrary::canDelete (const juce::File& file) const
{
    return file.existsAsFile() && rootFolder.isDirectory() && file.isAChildOf (rootFolder);
}

bool ReferenceAudioLibrary::deleteClip (const juce::File& file)
{
    if (! canDelete (file))
        return false;

    if (file == selectedFile)
    {
        unpinFile();
        clearSelection();
    }

    // The trash, so a wrong click can be undone in Finder. Where there is
    // no trash (some Linux desktops) the file is deleted outright - the
    // screen asked first.
    if (! file.moveToTrash() && ! file.deleteFile())
        return false;

    // A folder the import made, now empty, would stay in the list as a
    // category of nothing. A pack folder keeps its pack.json until its last
    // clip is gone, then goes the same way.
    for (auto folder = file.getParentDirectory(); folder.isAChildOf (rootFolder); folder = folder.getParentDirectory())
    {
        if (holdsAudio (folder))
            break;

        folder.deleteRecursively();
    }

    return true;
}

ReferenceAudioLibrary::Fragment ReferenceAudioLibrary::saveFragment (const juce::File& source,
                                                                     double startSeconds, double endSeconds)
{
    Fragment result;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));

    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return result;

    const auto rate = reader->sampleRate;
    const auto total = reader->lengthInSamples;

    if (endSeconds < startSeconds)
        std::swap (startSeconds, endSeconds);

    auto selStart = (juce::int64) (juce::jmax (0.0, startSeconds) * rate);
    auto selEnd = juce::jmin (total, (juce::int64) (endSeconds * rate));

    if (selEnd - selStart < (juce::int64) (0.2 * rate))
        return result;   // a click, not a selection

    // Read the selection with room around it: context for the tempo, slack
    // to move the ends, and what follows the end to fold into the seam.
    const auto context = (juce::int64) (8.0 * rate);
    const auto readStart = juce::jmax ((juce::int64) 0, selStart - context);
    const auto readEnd = juce::jmin (total, selEnd + context);
    const auto readLength = (int) juce::jmin ((juce::int64) (rate * 20.0 * 60.0), readEnd - readStart);

    juce::AudioBuffer<float> audio ((int) juce::jmax (1u, reader->numChannels), readLength);
    reader->read (&audio, 0, readLength, readStart, true, true);

    auto start = (int) (selStart - readStart);
    auto length = (int) (selEnd - selStart);

    AudioSliceAnalyzer::Options options;
    const auto tempo = AudioSliceAnalyzer::detectTempo (audio, rate, options);

    if (tempo.detected)
    {
        // Onto the grid: start on the nearest beat, length in whole bars
        // (whole beats below one bar - a single hit is a fair thing to want).
        const auto beat = 60.0 / tempo.bpm * rate;
        const auto bar = beat * options.beatsPerBar;
        const auto offset = std::round (((double) start - tempo.firstBeatSample) / beat);
        start = juce::jmax (0, (int) std::lround (tempo.firstBeatSample + offset * beat));

        const auto wantedBars = (double) length / bar;

        if (wantedBars >= 0.75)
        {
            result.bars = juce::jmax (1, (int) std::lround (wantedBars));
            length = (int) std::lround (result.bars * bar);
        }
        else
        {
            result.beats = juce::jmax (1, (int) std::lround ((double) length / beat));
            length = (int) std::lround (result.beats * beat);
        }

        // A grid that runs off the end of what was read shortens by a bar
        // rather than failing.
        while (start + length > audio.getNumSamples() && result.bars > 1)
        {
            --result.bars;
            length = (int) std::lround (result.bars * bar);
        }

        result.onBeatGrid = start + length <= audio.getNumSamples();
        result.bpm = tempo.bpm;
    }

    if (! result.onBeatGrid)
    {
        result.bars = result.beats = 0;
        length = (int) (selEnd - selStart);
        start = (int) (selStart - readStart);

        // Off the grid: each end to the quietest point within 100 ms.
        const auto quietest = [&] (int around)
        {
            const auto radius = (int) (0.1 * rate);
            const auto probe = juce::jmax (32, (int) (rate / 1000.0));
            auto best = around;
            auto bestEnergy = std::numeric_limits<float>::max();

            for (int p = juce::jmax (0, around - radius); p <= juce::jmin (audio.getNumSamples() - probe, around + radius); p += probe)
            {
                auto energy = 0.0f;
                for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                    energy += audio.getRMSLevel (ch, p, probe);

                if (energy < bestEnergy)
                {
                    bestEnergy = energy;
                    best = p;
                }
            }

            return best;
        };

        const auto end = quietest (start + length);
        start = quietest (start);
        length = end - start;
    }

    if (length <= (int) (0.1 * rate) || start + length > audio.getNumSamples())
        return result;

    // Where it goes: beside its source if that is already in the library,
    // else in the folder of the instrument it is.
    juce::File folder;

    if (source.isAChildOf (rootFolder))
    {
        folder = source.getParentDirectory();
    }
    else
    {
        const auto verdict = labelFor (source, audio, rate, { { start, length } }, (double) total / rate);
        folder = rootFolder.getChildFile (InstrumentLabel::folderNameFor (verdict.instrument));
    }

    if (! folder.createDirectory())
        return result;

    const auto destination = folder.getChildFile (source.getFileNameWithoutExtension() + " fragment.wav")
                                 .getNonexistentSibling();

    if (! writeLoopFile (audio, start, length, crossfadeFor (result.onBeatGrid, rate), rate, destination))
    {
        destination.deleteFile();
        return result;
    }

    result.file = destination;
    result.seconds = (double) length / rate;
    result.folderName = folder.getFileName();
    return result;
}

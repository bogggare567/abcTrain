#include "ReferenceAudioLibrary.h"
#include "AudioSliceAnalyzer.h"
#include "SampleBinaryData.h"
#include <array>

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
    addBuiltInCategories();

    if (! rootFolder.isDirectory())
        return;

    for (const auto& subDir : rootFolder.findChildFiles (juce::File::findDirectories, false))
    {
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
                category.files.add (file);
        }

        if (! category.files.isEmpty())
            categories.add (category);
    }
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
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr || reader->lengthInSamples <= 0)
            continue;   // not audio, or unreadable - skip it and carry on

        // Guard against a file so long it would not fit in memory. Twenty
        // minutes is more than any reasonable source and well inside what
        // a float buffer can hold.
        const auto maxSamples = (juce::int64) (reader->sampleRate * 20.0 * 60.0);
        const auto length = (int) juce::jmin (reader->lengthInSamples, maxSamples);

        juce::AudioBuffer<float> audio ((int) juce::jmax (1u, reader->numChannels), length);

        if (! reader->read (&audio, 0, length, 0, true, true))
            continue;

        const auto slices = AudioSliceAnalyzer::analyse (audio, reader->sampleRate);

        for (size_t i = 0; i < slices.size(); ++i)
        {
            const auto& slice = slices[i];

            const auto folder = rootFolder.getChildFile (AudioSliceAnalyzer::folderNameFor (slice.character));

            if (! folder.createDirectory())
                continue;

            const auto destination = folder.getChildFile (
                file.getFileNameWithoutExtension() + " " + juce::String ((int) i + 1) + ".wav")
                    .getNonexistentSibling();

            std::unique_ptr<juce::FileOutputStream> stream (destination.createOutputStream());

            if (stream == nullptr)
                continue;

            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer (
                // 16-bit, not 24: these are training loops, not masters.
                // The exercises hide changes of a decibel or more in them,
                // and 24-bit buys nothing against that while costing half
                // as much disk again.
                wav.createWriterFor (stream.get(), reader->sampleRate,
                                      (unsigned int) audio.getNumChannels(), 16, {}, 0));

            if (writer == nullptr)
                continue;

            stream.release();   // the writer owns it now

            juce::AudioBuffer<float> clip (audio.getNumChannels(), slice.numSamples);

            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                clip.copyFrom (channel, 0, audio, channel, slice.startSample, slice.numSamples);

            // A short fade at each end. Every clip here is going to be
            // looped, and a loop that starts or ends mid-waveform clicks on
            // every repeat - which the ear locks onto instead of the thing
            // being trained.
            const auto fadeSamples = juce::jmin (slice.numSamples / 8,
                                                  (int) (reader->sampleRate * 0.01));
            clip.applyGainRamp (0, fadeSamples, 0.0f, 1.0f);
            clip.applyGainRamp (slice.numSamples - fadeSamples, fadeSamples, 1.0f, 0.0f);

            if (writer->writeFromAudioSampleBuffer (clip, 0, clip.getNumSamples()))
                ++written;
        }
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

        written += importAndSlice (sources[i]);
    }

    if (onProgress != nullptr)
        onProgress (1.0f, {});

    return written;
}

void ReferenceAudioLibrary::setActiveCategory (const juce::String& categoryName, double sampleRate)
{
    activeCategory = categoryName;
    properties.setValue ("referenceCategory", activeCategory);
    properties.saveIfNeeded();

    advanceToRandomClip (sampleRate);
}

void ReferenceAudioLibrary::advanceToRandomClip (double sampleRate)
{
    if (activeCategory.isEmpty())
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

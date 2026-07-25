#include "ReferenceAudioLibrary.h"
#include "SampleBinaryData.h"
#include <array>

namespace
{
    constexpr const char* rootFolderKey = "referenceAudio.rootFolder";
    constexpr const char* selectedFileKey = "referenceAudio.selectedFile";

    juce::File defaultRootFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("ABCTrain");
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

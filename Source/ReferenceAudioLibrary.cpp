#include "ReferenceAudioLibrary.h"

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

void ReferenceAudioLibrary::rescan()
{
    categories.clear();

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

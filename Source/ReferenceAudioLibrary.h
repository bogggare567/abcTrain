#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>

// Lets a player practice EarTrainer's games on real reference audio they
// supply themselves (e.g. a folder of tracks organised one subfolder per
// genre) instead of only synthesized pink noise. This class only ever
// reads files that are already sitting on disk under a folder the user
// points it at - it never fetches, bundles, or otherwise supplies any
// audio content itself, and makes no attempt to verify the legality of
// whatever the user puts there; that's entirely the user's own
// responsibility, same as pointing any other audio tool at their own
// library. See decisions/015-choice-slider-and-training-sounds.md.
class ReferenceAudioLibrary
{
public:
    struct Category
    {
        juce::String name; // subfolder name, shown as-is in the UI
        juce::Array<juce::File> files;
    };

    // One shared PropertiesFile folder ("abcTrain"), same convention as
    // LocalisationManager::makeDefaultOptions() - the chosen root folder
    // and current selection are product-wide preferences, not per-plugin.
    static juce::PropertiesFile::Options makeDefaultOptions();

    explicit ReferenceAudioLibrary (juce::PropertiesFile& propertiesFile);

    // Where categories are scanned from. Defaults to the platform music
    // folder's "ABCTrain" subfolder the first time this ever runs;
    // persisted afterwards. Calling this rescans immediately.
    void setRootFolder (const juce::File& newRoot);
    juce::File getRootFolder() const noexcept { return rootFolder; }

    // Re-reads the root folder's immediate subdirectories as categories,
    // and each category's audio files (whatever
    // juce::AudioFormatManager's registered formats can actually open -
    // wav/aiff/mp3/flac/ogg). Does real file I/O - message thread only,
    // never the audio thread.
    void rescan();
    const juce::Array<Category>& getCategories() const noexcept { return categories; }

    // Loads `file` (message-thread only - blocking file I/O), downmixes
    // to mono, resamples to targetSampleRate if it differs from the
    // file's own rate, and publishes the result as the active reference
    // buffer every TestSignalGenerator reads from. Returns false (leaving
    // any previous selection untouched) if the file can't be read as
    // audio. Capped at maxBufferSeconds so a long track doesn't balloon
    // memory - the games loop it from the start once it runs out anyway.
    bool selectFile (const juce::File& file, double targetSampleRate);

    // Back to pink noise everywhere (see TestSignalGenerator).
    void clearSelection();

    juce::File getSelectedFile() const noexcept { return selectedFile; }

    // Called once by GameManager::prepare(), which is the first point the
    // real processing sample rate is known - re-loads whatever selection
    // was persisted from a previous session at the correct rate. A no-op
    // if nothing was ever selected.
    void prepare (double sampleRate);

    // Real-time safe: one atomic pointer load, no locks/allocation. Only
    // ever read from the audio thread (via TestSignalGenerator), never
    // written there.
    const juce::AudioBuffer<float>* getActiveBuffer() const noexcept { return activeBuffer.load(); }

    static constexpr double maxBufferSeconds = 20.0;

private:
    juce::PropertiesFile& properties;
    juce::AudioFormatManager formatManager;

    juce::File rootFolder;
    juce::Array<Category> categories;

    juce::File selectedFile;
    // Every loaded buffer is kept alive for the plugin instance's whole
    // lifetime instead of freed on the next selection - freeing one here
    // could race with the audio thread still mid-read of whatever
    // activeBuffer currently points at. A handful of ~20s mono buffers (a
    // few MB each) is a deliberately accepted memory tradeoff for a
    // simple, correct first pass - see decisions/015.
    juce::OwnedArray<juce::AudioBuffer<float>> loadedBuffers;
    std::atomic<const juce::AudioBuffer<float>*> activeBuffer { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceAudioLibrary)
};

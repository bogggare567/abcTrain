#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <functional>

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

    // Slices an imported file (or every audio file in a folder) into
    // loop-length clips and files each one under the category its
    // character puts it in - see AudioSliceAnalyzer.
    //
    // This is what makes "point it at your own music" actually work. Left
    // to itself, a folder of four-minute tracks gives the trainer
    // four-minute tracks: it will play the first eight seconds of a fade-in
    // over and over, and the exercise is useless. Slicing turns the same
    // folder into a sorted library of usable loops.
    //
    // Writes into rootFolder, never next to the original - the source
    // files are the player's own and are not touched, moved or renamed.
    // Returns how many clips were written; 0 means nothing usable was
    // found, which is an answer rather than a failure.
    //
    // Message thread only, and slow: it decodes and analyses whole files.
    // The caller is responsible for not doing this on the audio thread or
    // while the user expects the UI to respond.
    int importAndSlice (const juce::File& source);

    // The same work, over several files, safe to call from a background
    // thread. `onProgress` is called with 0..1 and the name of the file
    // being worked on; `shouldStop` is polled between files and between
    // clips so a long import can be abandoned when the window closes.
    //
    // Does **not** rescan - that touches state the message thread owns.
    // The caller rescans when it comes back.
    int importAndSliceMany (const juce::Array<juce::File>& sources,
                            std::function<void (float, juce::String)> onProgress,
                            std::function<bool()> shouldStop);

    // Where imported clips go. The app's own storage, not somewhere the
    // player has to find and pick: the folder-choosing step was a question
    // nobody wanted to answer before they could try the feature. A folder
    // *can* still be pointed at (setRootFolder), it just is not the way in
    // any more.
    static juce::File getManagedLibraryFolder();
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

    // Picks the category to train on, then a clip from it at random.
    //
    // Held as a *category* rather than a file because of what happens
    // next: advanceToRandomClip swaps in a different clip each round, so
    // twenty imported drum loops are twenty drum loops rather than the one
    // the player happened to land on. Repeating a single eight-second loop
    // for a whole session is the fastest way to stop hearing it.
    void setActiveCategory (const juce::String& categoryName, double sampleRate);
    juce::String getActiveCategory() const noexcept { return activeCategory; }

    // A different clip from the active category, avoiding an immediate
    // repeat where there is more than one to choose from. A no-op when
    // nothing is selected or the category has a single file.
    void advanceToRandomClip (double sampleRate);

    // Locks training to one specific clip, instead of rotating through the
    // category. Rotation is the right default - twenty imported drum loops
    // should be twenty drum loops - but "let me hear *that* one, over and
    // over" is a real request, and until now the only way to answer it was
    // to put a single file in a folder of its own.
    void pinFile (const juce::File&, double sampleRate);
    void unpinFile();
    bool isPinned() const noexcept { return pinned; }

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
    // Re-materialises the small set of embedded, originally-synthesized
    // WAV samples (SampleBinaryData, see CMakeLists.txt) as real files
    // under a cache directory, then adds them to `categories` as one or
    // more "Built-in ..." categories, ahead of anything scanned from
    // `rootFolder` - so there's always at least one real (if simple)
    // training option even with no folder configured at all. Cache files
    // let this reuse selectFile()'s ordinary File-based path unchanged,
    // rather than duplicating it for an in-memory source. See decisions/018.
    void addBuiltInCategories();

    juce::PropertiesFile& properties;
    juce::AudioFormatManager formatManager;

    juce::File rootFolder;
    juce::Array<Category> categories;

    juce::File selectedFile;
    juce::String activeCategory;
    bool pinned = false;
    juce::Random random;
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

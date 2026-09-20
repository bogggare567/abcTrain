#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <functional>

// Splits a piece of music into four stems with classic signal processing:
// drums, bass, centre (vocals & leads) and sides (wide & ambience).
//
// **What this is.** Two old, well-understood tricks, stacked:
//
//  1. Harmonic/percussive separation by median filtering (Fitzgerald,
//     2010). In a spectrogram a held note is a horizontal line and a hit is
//     a vertical one. A median taken *across time* keeps the lines and
//     loses the hits; a median taken *across frequency* keeps the hits and
//     loses the lines. Each becomes a soft mask, and the two masks share
//     every bin between them.
//  2. Stereo position (Avendano & Jot, 2002). Per bin, how alike are left
//     and right - 2·Re(L·R*) / (|L|² + |R|²), averaged over a fraction of a
//     second so it measures coherence and not just level balance. Near 1
//     means "in the middle"; well below it means panned, decorrelated,
//     out of phase, or room. (The paper uses |L·R*|; the real part is used
//     here so anti-phase content counts as wide, which is what it sounds.)
//
// Percussive comes out first as `drums`. What is left is harmonic; below
// ~180 Hz (a smooth crossover, not a brick wall) that is `bass`, and the
// rest is divided by stereo position into `centre` and `sides`.
//
// **What this is not.** It is not Demucs, Spleeter or any trained model,
// and it does not know what a voice or a drum is. `centre` is literally
// "what is in the middle of the stereo image" - in most commercial mixes
// that is the lead vocal, but it is also the kick, the snare and the bass,
// which is exactly why percussive and low harmonic content are taken out
// *before* the centre/sides split rather than after. Expect bleed: a
// sustained synth in the middle lands in `centre` with the vocal, a snare's
// ringing tail can land in `centre` or `sides`, a strummed guitar splits
// between `drums` and wherever it is panned. A mono file has no stereo
// position to go on, so everything harmonic above the bass goes to
// `centre` and `sides` is silent. That is the honest answer, not a bug.
//
// **What it guarantees.** The four masks sum to exactly 1 in every bin and
// the STFT is overlap-added with its own normalisation, so the stems add
// back up to the input (to float precision). Nothing is invented or lost;
// it is only divided up.
//
// Pure DSP over a buffer: no Component, no file I/O, no message loop, so
// tests drive it with synthesized signals whose right answer is known by
// construction. Internally streaming - the spectrogram is never held in
// full, only the few frames the median filter needs - so memory is the
// input plus the four output stems.
namespace StemSeparator
{
    enum class Stem
    {
        drums,      // percussive: whatever is vertical in the spectrogram
        bass,       // harmonic content below the crossover
        centre,     // harmonic, above the crossover, in the middle - usually the vocal
        sides       // harmonic, above the crossover, panned or wide - pads, room, doubles
    };

    constexpr int numStems = 4;

    struct Options
    {
        // Where bass hands over to centre/sides. The mask is a smooth
        // eighth-order curve (0.5 at this frequency), not a brick wall -
        // a hard cut in a spectral mask rings audibly.
        double bassCrossoverHz = 180.0;

        // Length of the median across time, in seconds. Longer keeps only
        // notes that are held longer before calling them harmonic.
        double harmonicMedianSeconds = 0.4;

        // Width of the median across frequency, in Hz. Wider needs a hit
        // to be broader-band before calling it percussive.
        double percussiveMedianHz = 250.0;

        // Exponent of the soft (Wiener-style) masks. 2 is Fitzgerald's
        // choice; higher is closer to a binary mask and more artefact-prone.
        float maskPower = 2.0f;

        // How alike L and R have to be to count as centre. Below `low` a
        // bin goes to sides, above `high` to centre, smoothly in between.
        // 0.75 (the midpoint) is a source panned roughly halfway out.
        float centreSimilarityLow  = 0.55f;
        float centreSimilarityHigh = 0.95f;
    };

    struct Result
    {
        // Indexed by (int) Stem. Each has the input's channel count (mono
        // or stereo; a file with more than two channels is treated as its
        // first two) and length.
        std::array<juce::AudioBuffer<float>, numStems> stems;

        // False when shouldStop cut the work short - the stems are then
        // empty rather than half-written.
        bool completed = false;

        const juce::AudioBuffer<float>& get (Stem s) const noexcept { return stems[(size_t) s]; }
    };

    // Separates `audio`. `onProgress` is called with 0..1 now and then;
    // `shouldStop` is polled every few dozen frames. Either may be empty.
    //
    // An empty buffer or a non-positive sample rate gives a completed
    // result with empty stems - a harmless miss, not an error.
    Result separate (const juce::AudioBuffer<float>& audio,
                     double sampleRate,
                     const Options& options = {},
                     std::function<void (float)> onProgress = {},
                     std::function<bool()> shouldStop = {});

    // The library folder a stem's clips are filed under. Stable across
    // releases for the same reason AudioSliceAnalyzer's are:
    // ReferenceAudioLibrary scans by folder name, so renaming one orphans
    // everything already sorted into it.
    const char* folderNameFor (Stem) noexcept;
}

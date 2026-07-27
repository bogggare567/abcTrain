#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

// Takes a long piece of audio the player has imported and works out where
// to cut it and what each piece is good for practising.
//
// The point: the reference-audio feature only pays off if dropping a
// folder of your own music in front of it produces something usable. A
// four-minute track is not a training signal - you need eight seconds that
// starts on a downbeat, loops without a click, and is *about* something,
// so the exercise has a consistent thing to hide a change inside.
//
// **What this does not do, and cannot.** It does not separate a vocal from
// a mix, or drums from everything else. That is source separation - a
// machine-learning problem needing a trained model, not a few spectral
// measurements - and a heuristic pretending to do it would mislabel most
// real music while sounding confident about it. What it does instead is
// sort by *measurable character*: how transient-heavy a passage is, where
// its energy sits, how wide it is. Those are the properties that decide
// which exercise a clip is useful for, which is the actual question.
//
// So a drum loop lands in `percussive` because it is full of transients,
// not because anything here knows what a drum is. A lead vocal in a sparse
// arrangement will usually land in `midRange`; a lead vocal over a dense
// mix will land in `fullRange`, correctly, because that is what the audio
// is.
//
// Pure DSP over a buffer: no Component, no file I/O, no message loop, so
// tests drive it directly with synthesized signals.
namespace AudioSliceAnalyzer
{
    enum class Character
    {
        // Dense transients - drum loops, percussion, plucked rhythm parts.
        // The best material for compression, delay and distortion, where
        // hearing an envelope change is the whole exercise.
        percussive,

        // Energy concentrated low. Bass lines, kick-led passages. Good for
        // the frequency exercises at the bottom of the range, useless for
        // anything about air.
        bass,

        // Energy concentrated where voices and most lead instruments sit.
        // The general-purpose choice.
        midRange,

        // Energy concentrated high - cymbals, air, bright synths.
        bright,

        // Broad and even, and usually wide. A finished mix. The realistic
        // case, and the hardest to hear a small change inside.
        fullRange
    };

    // A cut, in samples, plus what it turned out to be.
    struct Slice
    {
        int startSample = 0;
        int numSamples = 0;
        Character character = Character::fullRange;

        // The measurements the character was decided from - exposed so a
        // UI can show *why* a clip was sorted where it was, and so a test
        // can check the classifier without guessing at its internals.
        float onsetsPerSecond = 0.0f;
        float spectralCentroidHz = 0.0f;
        float stereoCorrelation = 1.0f;
        float rms = 0.0f;
    };

    struct Options
    {
        // Loop length. Eight seconds is long enough to contain a musical
        // phrase and short enough that a player is not waiting through it
        // to answer.
        double sliceSeconds = 8.0;

        // Slices quieter than this are dropped: intros, fades and gaps
        // make useless training material, and a silent "clip" in the
        // library is worse than no clip.
        float minimumRms = 0.02f;

        // How far the analyser may move a cut to land it on a quiet point,
        // as a fraction of sliceSeconds. Cutting mid-note gives a loop
        // that clicks on every repeat.
        double snapWindow = 0.15;
    };

    // Every usable slice, in order. An empty result means the audio was
    // too short or too quiet to get anything out of - which is a real
    // answer, not a failure.
    std::vector<Slice> analyse (const juce::AudioBuffer<float>& audio,
                                double sampleRate,
                                const Options& options = {});

    // The folder name a slice of this character belongs in. Stable across
    // releases - ReferenceAudioLibrary scans by folder name, so renaming
    // one orphans everything already sorted into it.
    const char* folderNameFor (Character) noexcept;

    // The i18n key for a human-readable name, resolved by the caller (this
    // namespace stays free of LocalisationManager like the rest of the
    // non-UI code).
    const char* nameKeyFor (Character) noexcept;
}

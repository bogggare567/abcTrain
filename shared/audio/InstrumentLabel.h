#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <optional>
#include <vector>

// What instrument a piece of audio is - from its file name first, then
// checked against how it sounds - and "Other" whenever that is not certain.
//
// The same rules as tools/library/prepare_audio.py (the pack builder), so a
// clip the player imports lands in the same place a clip in a pack does.
// Change one, change the other: the keyword tables and the thresholds below
// are copies, and nothing but a reader keeps them in step.
//
// Why the name comes first. The honest limits of this are in
// AudioSliceAnalyzer.h and docs/design/sound-library.md: telling a guitar
// from a keyboard from a voice by ear is a trained model's job, and a few
// spectral measurements pretending to do it would be confidently wrong. What
// measurements *can* do reliably is the physical part - a kick is thumps
// and low end, a hi-hat is thumps and air, a bass line is low and sustained,
// a finished mix is broad and wide. So: a file called "kick in" is a kick
// unless it plainly sounds like something else; a file called "Audio 3" is
// whatever it sounds like *if* that is one of the things sound can tell; and
// otherwise it is "Other", with the reason written down. A clip in "Other"
// the player can move is better than a clip in "Guitar" that is a vocal.
namespace InstrumentLabel
{
    // Stable ids: folder names and i18n keys are built from them.
    enum class Instrument
    {
        kick, snare, hihat, cymbals, toms, percussion,
        bass, guitar, keys, vocal, wind,
        mix, other
    };

    constexpr int numInstruments = 13;

    enum class Family { drums, tonal, mix, other };
    Family familyOf (Instrument) noexcept;

    // "kick", "hihat" ... - the id prepare_audio.py writes as the pack's
    // subfolder and the key under "sounds.instrument.".
    const char* idOf (Instrument) noexcept;
    std::optional<Instrument> fromId (const juce::String&);

    // The folder an imported clip goes in. English and stable: the library
    // scans by folder name, and renaming one orphans everything in it. The
    // screen shows the translated name (nameKeyFor), not this.
    const char* folderNameFor (Instrument) noexcept;
    const char* nameKeyFor (Instrument) noexcept;

    // Is `folderName` one of ours - so the screen may translate it? A
    // folder the player made keeps the name they gave it.
    std::optional<Instrument> fromFolderName (const juce::String& folderName);

    struct NameHit
    {
        Instrument instrument;
        juce::String word;   // what in the name said so, for the report
    };

    // From a file name (without the folder). Words, not substrings: "bd",
    // "oh", "ana" inside other words mean anything at all.
    std::optional<NameHit> fromName (const juce::String& fileName);

    // What the audio measures like. Mirrors prepare_audio.Features.
    struct Features
    {
        float centroidHz = 0.0f;
        float onsetsPerSecond = 0.0f;
        float sustainFraction = 0.0f;
        float bassFraction = 0.0f, midFraction = 0.0f, highFraction = 0.0f;   // by magnitude, 250 / 4000 Hz
        float subFraction = 0.0f, lowMidFraction = 0.0f, airFraction = 0.0f;  // by power, 120 / 400 / 6000 Hz
        float flatness = 0.0f;       // 1 = noise, 0 = tone
        float pitchedness = 0.0f;    // autocorrelation peak, 50-1000 Hz
        float crestDb = 0.0f;
        float stereoCorrelation = 1.0f;

        float width() const noexcept { return juce::jmax (0.0f, 1.0f - stereoCorrelation); }
    };

    // Message thread or a worker - never the audio thread (FFTs, vectors).
    Features measure (const juce::AudioBuffer<float>& audio, int start, int numSamples, double sampleRate);

    // Median of each field: a file is judged by all its clips, not one.
    Features median (const std::vector<Features>&);

    struct Verdict
    {
        Instrument instrument = Instrument::other;
        float confidence = 0.0f;
        juce::String reason;         // one line, English, for logs and tests
        Instrument bestGuess = Instrument::other;
    };

    // `songShaped`: a single file one to twenty minutes long, not a stem of
    // a multitrack - almost always a finished song, so a mix unless the
    // name or the sound clearly says otherwise.
    Verdict decide (const std::optional<NameHit>&, const Features&, bool songShaped);

    // Below this the sound alone does not name anything.
    constexpr float minimumConfidence = 0.6f;
}

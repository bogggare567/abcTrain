#pragma once

#include <juce_core/juce_core.h>
#include <array>

// Contextual tooltip text per parameter, and the four teaching presets.
// Shared by the processor (applyPreset) and the editor (guide label,
// preset buttons).
namespace CompressorGuide
{
    struct Preset
    {
        const char* name;

        // One sentence on what this setting is *for*, shown in the guide
        // card when the preset is applied. A preset that silently moves
        // seven knobs teaches nothing; a preset that says why it moved
        // them is the entire point of a plugin called "Learner".
        const char* what;
        float thresholdDb;
        float ratio;
        float attackMs;
        float releaseMs;
        float kneeDb;
    };

    inline const std::array<Preset, 4> presets {{
        { "Vocal Smoothing",
          "Moderate ratio with a soft knee and a fast-but-not-instant attack: "
          "it evens out a performance without flattening the consonants that "
          "carry the words.",
          -18.0f, 3.0f,  5.0f,  150.0f, 12.0f },
        { "Punchy Drums",
          "A slow 20 ms attack deliberately lets the stick hit through before "
          "the compressor closes, so the transient stays sharp and only the "
          "body behind it is controlled.",
          -12.0f, 4.0f,  20.0f, 80.0f,  0.0f  },
        { "Bass Control",
          "Low threshold and a long release, so the gain settles rather than "
          "moving with each note - a fast release on bass audibly pumps in "
          "time with the part.",
          -24.0f, 6.0f,  10.0f, 200.0f, 6.0f  },
        { "Limiter",
          "A very high ratio with near-zero attack: a ceiling rather than a "
          "compressor. Useful to hear what over-limiting costs you - listen "
          "for the life going out of the dynamics.",
          -6.0f,  20.0f, 0.1f,  50.0f,  0.0f  }
    }};

    // Text here mirrors docs/knowledge_base.md's "Компрессия и
    // динамическая обработка" section - keep both in sync. Original
    // explanations from general, widely-taught practice, not derived
    // from any specific book (see decisions/010-book-library-scope.md).
    // "Learn more" names one relevant book from docs/library_catalog.md -
    // title/author only, never a quote from it.
    inline juce::String describe (const juce::String& paramId)
    {
        if (paramId == "threshold")
            return "The level above which the compressor starts working. "
                   "-12 dB means only peaks louder than -12 dB get turned "
                   "down; everything quieter passes through untouched. "
                   "Lower thresholds mean more of the signal gets compressed - "
                   "worth choosing together with ratio and knee rather than "
                   "dialing each in isolation.\n"
                   "Learn more: Roey Izhaki - Mixing Audio.";
        if (paramId == "ratio")
            return "How much compression is applied above the threshold. "
                   "4:1 means 4 dB over the threshold becomes 1 dB at the "
                   "output. 2-4:1 reads as gentle leveling; 8:1 and up starts "
                   "to feel like limiting - though a high ratio alone won't "
                   "sound obviously hard without a threshold set to match it.\n"
                   "Learn more: Bobby Owsinski - The Mixing Engineer's Handbook.";
        if (paramId == "attack")
            return "How fast compression kicks in after a peak crosses the "
                   "threshold. A fast attack (0.1-5 ms) catches transients and "
                   "controls them; a slower attack (20-30 ms) lets the initial "
                   "hit through before clamping down - the classic way to keep "
                   "drums punchy while still taming the sustain. A good habit is "
                   "starting slow and only speeding up as much as actually "
                   "needed, since an extremely fast attack can visibly distort a "
                   "low-frequency waveform.\n"
                   "Learn more: Bobby Owsinski - The Mixing Engineer's Handbook.";
        if (paramId == "release")
            return "How fast compression lets go after the signal drops back "
                   "below threshold. Too short on fast material can cause "
                   "audible pumping (gain visibly breathing with the rhythm); "
                   "too long can feel sluggish, not recovering before the next "
                   "peak arrives. Watching the gain-reduction meter's movement "
                   "over time, not just its peak value, is the more reliable way "
                   "to judge whether release is actually right.\n"
                   "Learn more: Roey Izhaki - Mixing Audio.";
        if (paramId == "knee")
            return "How gradually compression ramps in around the threshold. "
                   "0 dB is a hard knee - compression switches on suddenly "
                   "right at the threshold; higher values ease in gradually "
                   "before and after it, which usually sounds smoother and "
                   "less noticeable on program material - though a very wide "
                   "knee starts shaping the signal noticeably below the "
                   "displayed threshold number.\n"
                   "Learn more: Bobby Owsinski - The Mixing Engineer's Handbook.";
        if (paramId == "makeup")
            return "Restores loudness lost to compression - it doesn't change "
                   "the dynamics, only the overall level afterward. Raise it "
                   "to bring a compressed signal back up to match (or exceed) "
                   "the original's perceived loudness. Comparing compressed and "
                   "dry signal at mismatched volumes is one of the easiest ways "
                   "to fool yourself about whether the compression itself is "
                   "actually doing anything useful.\n"
                   "Learn more: Bob Katz - Mastering Audio: The Art and the Science.";
        if (paramId == "dryWet")
            return "Blends compressed (wet) with uncompressed (dry) signal. "
                   "Parallel compression - wet below 100% - adds density and "
                   "sustain from the compressed signal while keeping the dry "
                   "signal's original transients and dynamics intact. Push the "
                   "wet amount too far and fast material can turn to mush - a "
                   "modest blend usually keeps most of the benefit.\n"
                   "Learn more: Roey Izhaki - Mixing Audio.";

        return {};
    }
}

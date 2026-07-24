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
        float thresholdDb;
        float ratio;
        float attackMs;
        float releaseMs;
        float kneeDb;
    };

    inline const std::array<Preset, 4> presets {{
        { "Vocal Smoothing", -18.0f, 3.0f,  5.0f,  150.0f, 12.0f },
        { "Punchy Drums",    -12.0f, 4.0f,  20.0f, 80.0f,  0.0f  },
        { "Bass Control",    -24.0f, 6.0f,  10.0f, 200.0f, 6.0f  },
        { "Limiter",         -6.0f,  20.0f, 0.1f,  50.0f,  0.0f  }
    }};

    inline juce::String describe (const juce::String& paramId)
    {
        if (paramId == "threshold")
            return "The level above which the compressor starts working. "
                   "-12 dB means peaks louder than -12 dB get compressed.";
        if (paramId == "ratio")
            return "How much compression is applied. 4:1 means 4 dB over "
                   "the threshold becomes 1 dB at the output.";
        if (paramId == "attack")
            return "How fast compression kicks in. A fast attack can flatten "
                   "transients; a slow attack lets the initial hit through.";
        if (paramId == "release")
            return "How fast compression lets go. Too short can cause audible "
                   "pumping; too long can feel sluggish.";
        if (paramId == "knee")
            return "How gradually compression ramps in around the threshold. "
                   "0 dB is a hard knee (sudden); higher values ease in more gently.";
        if (paramId == "makeup")
            return "Restores loudness lost to compression. Raise it to bring "
                   "the level back up after the peaks are tamed.";
        if (paramId == "dryWet")
            return "Blends compressed (wet) with uncompressed (dry) signal - "
                   "parallel compression at less than 100%.";

        return {};
    }
}

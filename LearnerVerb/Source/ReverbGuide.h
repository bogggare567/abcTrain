#pragma once

#include <juce_core/juce_core.h>
#include <array>

// Contextual tooltip text per parameter, and the four teaching presets.
// Shared by the processor (applyPreset) and the editor (guide label,
// preset buttons) - same pattern as LearnerComp's CompressorGuide.
namespace ReverbGuide
{
    struct Preset
    {
        const char* name;
        int typeIndex; // 0=Room, 1=Hall, 2=Plate, 3=Spring
        float decaySeconds;
        float preDelayMs;
        float size;
        float damping;
        float dryWetPercent;
        float width;
    };

    inline const std::array<Preset, 4> presets {{
        { "Vocal Ambience", 2, 1.5f, 40.0f, 0.5f, 0.3f, 25.0f, 1.0f },
        { "Concert Hall",   1, 2.5f, 20.0f, 0.8f, 0.4f, 30.0f, 1.0f },
        { "Small Room",     0, 0.6f, 10.0f, 0.3f, 0.5f, 20.0f, 0.7f },
        { "Spring Tank",    3, 2.0f, 0.0f,  0.5f, 0.3f, 35.0f, 0.5f }
    }};

    // Text here mirrors docs/knowledge_base.md's "Реверберация и
    // пространство" section - keep both in sync. Original explanations
    // from general, widely-taught practice, not derived from any
    // specific book (see decisions/010-book-library-scope.md). "Learn
    // more" names one relevant book from docs/library_catalog.md -
    // title/author only, never a quote from it.
    inline juce::String describe (const juce::String& paramId)
    {
        if (paramId == "type")
            return "The reverb algorithm: Room and Hall are size-based spaces "
                   "(Room short and dense, Hall long with a smooth, spacious "
                   "decay), Plate is a bright, smooth studio character with no "
                   "real-room geometry, Spring is the metallic, clanging "
                   "character of a guitar-amp spring tank.\n"
                   "Learn more: F. Alton Everest - Master Handbook of Acoustics.";
        if (paramId == "decay")
            return "How long the reverb tail takes to fade out. Longer decay "
                   "sounds like a bigger, more reflective space; 1.5-2.5 s is "
                   "typical for vocal ambience, while shorter times (0.3-0.8 s) "
                   "keep a dense mix from getting washed out.\n"
                   "Learn more: F. Alton Everest - Master Handbook of Acoustics.";
        if (paramId == "preDelay")
            return "The gap between the dry sound and when the reverb starts. "
                   "20-50 ms keeps a vocal's consonants clear before the space "
                   "blooms in - without it, the tail can swallow the start of "
                   "the sound.\n"
                   "Learn more: Philip Newell - Recording Studio Design.";
        if (paramId == "size")
            return "How large the simulated space is - affects the density and "
                   "spacing of reflections, not just the decay time. A bigger "
                   "size feels like a physically larger room even at the same "
                   "decay setting.\n"
                   "Learn more: F. Alton Everest - Master Handbook of Acoustics.";
        if (paramId == "damping")
            return "How quickly high frequencies die out in the tail. More "
                   "damping sounds darker and more absorbent, like a room full "
                   "of soft furnishings; less damping sounds brighter and more "
                   "metallic, like a hard-surfaced space.\n"
                   "Learn more: F. Alton Everest - Master Handbook of Acoustics.";
        if (paramId == "dryWet")
            return "Blends the reverberated (wet) signal with the untreated "
                   "(dry) source. Lower wet amounts keep the source upfront "
                   "with just a hint of space; higher amounts push it further "
                   "back.\n"
                   "Learn more: Roey Izhaki - Mixing Audio.";
        if (paramId == "width")
            return "How wide the reverb's stereo image is, independent of the "
                   "dry signal's width - a narrower tail keeps the space subtle "
                   "and centered, a wider one surrounds the source.\n"
                   "Learn more: Roey Izhaki - Mixing Audio.";

        return {};
    }
}

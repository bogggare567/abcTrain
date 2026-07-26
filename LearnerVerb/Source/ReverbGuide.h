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

        // One sentence on what this setting is *for*, shown in the guide
        // card when the preset is applied - see the same field on
        // LearnerComp's CompressorGuide::Preset.
        const char* what;
        int typeIndex; // 0=Room, 1=Hall, 2=Plate, 3=Spring
        float decaySeconds;
        float preDelayMs;
        float size;
        float damping;
        float dryWetPercent;
        float width;
    };

    inline const std::array<Preset, 4> presets {{
        { "Vocal Ambience",
          "A short plate with 40 ms of pre-delay. The gap before the tail "
          "starts is what keeps the voice in front of the reverb instead of "
          "inside it.",
          2, 1.5f, 40.0f, 0.5f, 0.3f, 25.0f, 1.0f },
        { "Concert Hall",
          "Long decay, large size, wide. Notice how much of the sense of "
          "space comes from the size rather than the decay time alone.",
          1, 2.5f, 20.0f, 0.8f, 0.4f, 30.0f, 1.0f },
        { "Small Room",
          "Under a second of decay and higher damping - it reads as a room "
          "rather than as an effect. Small rooms are mostly early "
          "reflections, which is why this still sounds close.",
          0, 0.6f, 10.0f, 0.3f, 0.5f, 20.0f, 0.7f },
        { "Spring Tank",
          "The metallic, springy character you know from guitar amps. It "
          "comes from resonant allpass filters, not from a modelled room, "
          "which is why it colours the sound rather than placing it.",
          3, 2.0f, 0.0f,  0.5f, 0.3f, 35.0f, 0.5f }
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
                   "character of a guitar-amp spring tank. Reusing the same "
                   "type/settings across several tracks (rather than a unique "
                   "reverb per source) often reads as a more cohesive shared "
                   "space in a full mix.\n"
                   "Learn more: F. Alton Everest - Master Handbook of Acoustics.";
        if (paramId == "decay")
            return "How long the reverb tail takes to fade out. Longer decay "
                   "sounds like a bigger, more reflective space; 1.5-2.5 s is "
                   "typical for vocal ambience, while shorter times (0.3-0.8 s) "
                   "keep a dense mix from getting washed out. In a real space "
                   "the high end usually dies out faster than the low end - "
                   "Damping below models that independently of this overall "
                   "decay time.\n"
                   "Learn more: F. Alton Everest - Master Handbook of Acoustics.";
        if (paramId == "preDelay")
            return "The gap between the dry sound and when the reverb starts. "
                   "20-50 ms keeps a vocal's consonants clear before the space "
                   "blooms in - without it, the tail can swallow the start of "
                   "the sound. It's often tuned together with Size and Decay as "
                   "one bigger, longer, later-starting space, rather than moved "
                   "on its own.\n"
                   "Learn more: Philip Newell - Recording Studio Design.";
        if (paramId == "size")
            return "How large the simulated space is - affects the density and "
                   "spacing of reflections, not just the decay time. A bigger "
                   "size feels like a physically larger room even at the same "
                   "decay setting, though pushed too far it can blur a "
                   "transient's sharp attack into the reflections themselves.\n"
                   "Learn more: F. Alton Everest - Master Handbook of Acoustics.";
        if (paramId == "damping")
            return "How quickly high frequencies die out in the tail. More "
                   "damping sounds darker and more absorbent, like a room full "
                   "of soft furnishings; less damping sounds brighter and more "
                   "metallic, like a hard-surfaced space. Too little damping on "
                   "a long decay can leave a bright, harsh tail hanging around "
                   "far longer than the low end needs to.\n"
                   "Learn more: F. Alton Everest - Master Handbook of Acoustics.";
        if (paramId == "dryWet")
            return "Blends the reverberated (wet) signal with the untreated "
                   "(dry) source. Lower wet amounts keep the source upfront "
                   "with just a hint of space; higher amounts push it further "
                   "back. When a reverb is shared across several tracks via a "
                   "send, the common convention is to run the return itself "
                   "near 100% wet and do the dry/wet blending per-track "
                   "instead, right here.\n"
                   "Learn more: Roey Izhaki - Mixing Audio.";
        if (paramId == "width")
            return "How wide the reverb's stereo image is, independent of the "
                   "dry signal's width - a narrower tail keeps the space subtle "
                   "and centered, a wider one surrounds the source. Controlling "
                   "width here directly is generally safer than panning the wet "
                   "return itself, which can introduce its own left/right phase "
                   "issues - always worth a quick mono check either way.\n"
                   "Learn more: Roey Izhaki - Mixing Audio.";

        return {};
    }
}

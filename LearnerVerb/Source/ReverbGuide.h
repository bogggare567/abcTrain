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

    inline juce::String describe (const juce::String& paramId)
    {
        if (paramId == "type")
            return "The reverb algorithm: Room and Hall are size-based spaces, "
                   "Plate is a bright studio plate, Spring is a metallic tank character.";
        if (paramId == "decay")
            return "How long the reverb tail takes to fade out. Longer decay "
                   "sounds like a bigger, more reflective space.";
        if (paramId == "preDelay")
            return "The gap between the dry sound and when the reverb starts. "
                   "A longer pre-delay keeps the source clear before the space blooms.";
        if (paramId == "size")
            return "How large the simulated space is - affects the density and "
                   "spacing of reflections, not just the decay time.";
        if (paramId == "damping")
            return "How quickly high frequencies die out in the tail. More damping "
                   "sounds darker and more absorbent; less sounds brighter and more metallic.";
        if (paramId == "dryWet")
            return "Blends the reverberated (wet) signal with the untreated (dry) "
                   "source.";
        if (paramId == "width")
            return "How wide the reverb's stereo image is, independent of the dry signal's width.";

        return {};
    }
}

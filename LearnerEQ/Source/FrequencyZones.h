#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// The spectrum, labelled in what things *sound like* rather than in
// numbers alone.
//
// The reason this exists: "cut 300 Hz" is a instruction you can follow
// without learning anything. What a mixer actually carries in their head
// is a map of sensations - where a mix gets boxy, where a voice gets
// honky, where sibilance lives, where "air" is - and the number is how you
// write that sensation down afterwards. An EQ that shows only a log axis
// is asking you to have the map already.
//
// So the display draws named zones behind the curve. They are deliberately
// *fuzzy and overlapping* in reality; the boundaries below are the
// conventional teaching ones, and the tooltip text says so rather than
// pretending 250 Hz is a wall. Names and boundaries match the ones
// docs/knowledge_base.md and EarTrainer's FrequencyRangeGame already use,
// so the same vocabulary is taught in both halves of the product.
namespace FrequencyZones
{
    struct Zone
    {
        float lowHz;
        float highHz;
        const char* name;      // what it is called
        const char* feels;     // what too much of it does - the useful half
    };

    // Eight zones from 20 Hz to 20 kHz, contiguous so there is never a gap
    // the pointer can fall into.
    inline constexpr std::array<Zone, 8> all
    {{
        {   20.0f,    60.0f, "Sub",       "felt more than heard - rumble" },
        {   60.0f,   120.0f, "Bass",      "weight; too much and it booms" },
        {  120.0f,   300.0f, "Boom",      "warmth, then mud and boxiness" },
        {  300.0f,   700.0f, "Body",      "fullness; too much sounds cardboard" },
        {  700.0f,  2000.0f, "Honk",      "where a voice starts to nasal" },
        { 2000.0f,  5000.0f, "Presence",  "clarity and bite - and listening fatigue" },
        { 5000.0f,  9000.0f, "Sibilance", "consonants; harshness and ess" },
        { 9000.0f, 20000.0f, "Air",       "openness, sheen, the top of a cymbal" }
    }};

    inline const Zone& zoneFor (float freqHz)
    {
        for (const auto& zone : all)
            if (freqHz < zone.highHz)
                return zone;

        return all.back();
    }

    // Alternating tint so neighbours are distinguishable without eight
    // different colours, which would turn the analyser into a rainbow and
    // fight the curve for attention. The zone under the pointer gets the
    // accent instead - see the display.
    inline float shadeFor (int zoneIndex)
    {
        return (zoneIndex % 2 == 0) ? 0.05f : 0.018f;
    }
}

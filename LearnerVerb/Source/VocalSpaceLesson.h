#pragma once

#include "../../shared/MicroLesson.h"
#include "PluginProcessor.h"

// Lesson content lives per-plugin, not in shared/ - see ADR 005. Type
// indices match the AudioParameterChoice order in PluginProcessor.cpp:
// 0 = Room, 1 = Hall, 2 = Plate, 3 = Spring.
inline MicroLesson buildVocalSpaceLesson()
{
    using P = LearnerVerbProcessor;

    return MicroLesson ("Space for Vocals", {
        { "Dry signal, no reverb.",
          { { P::dryWetParamId, 0.0f } } },
        { "Add Plate reverb: 1.5 s decay, 20% wet.",
          { { P::typeParamId, 2.0f }, { P::decayParamId, 1.5f }, { P::dryWetParamId, 20.0f } } },
        { "Pre-Delay 40 ms - separate the voice from the reverb before it blooms.",
          { { P::preDelayParamId, 40.0f } } },
        { "Increase damping - remove brightness from the tail.",
          { { P::dampingParamId, 70.0f } } },
        { "Compare with Hall at 2.5 s decay - a different kind of space entirely.",
          { { P::typeParamId, 1.0f }, { P::decayParamId, 2.5f } } }
    });
}

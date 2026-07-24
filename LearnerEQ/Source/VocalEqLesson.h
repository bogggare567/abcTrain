#pragma once

#include "../../shared/MicroLesson.h"
#include "PluginProcessor.h"

// Lesson content lives per-plugin, not in shared/ - only the MicroLesson/
// LessonController machinery is shared; the steps themselves are specific
// to this plugin's parameters (see ADR 005 for why).
inline MicroLesson buildVocalEqLesson()
{
    using P = LearnerEQProcessor;

    return MicroLesson ("Vocal EQ Basics", {
        { "This is the untreated sound - every band flat.",
          { { P::gainParamId (0), 0.0f }, { P::gainParamId (1), 0.0f },
            { P::gainParamId (2), 0.0f }, { P::gainParamId (3), 0.0f } } },
        { "Add presence: boost 3 kHz by +3 dB.",
          { { P::freqParamId (2), 3000.0f }, { P::gainParamId (2), 3.0f } } },
        { "Clean up mud: cut 250 Hz by -3 dB.",
          { { P::freqParamId (1), 250.0f }, { P::gainParamId (1), -3.0f } } },
        { "Add air: boost 10 kHz (the high shelf) by +2 dB.",
          { { P::freqParamId (3), 10000.0f }, { P::gainParamId (3), 2.0f } } },
        { "Compare: go back a step to hear the difference, or close the lesson to keep this EQ curve.",
          {} }
    });
}

#pragma once

#include "../../shared/MicroLesson.h"
#include "PluginProcessor.h"

// Second LearnerEQ lesson (see decisions/017): the "boost narrow, find the
// resonance, then flip to a cut" technique from docs/knowledge_base.md's
// "Эквализация" section - a different skill than buildVocalEqLesson()'s
// straight tonal-shaping walkthrough.
inline MicroLesson buildFindResonanceLesson()
{
    using P = LearnerEQProcessor;

    return MicroLesson ("Find & Fix a Resonance", {
        { "Start flat - every band at zero, ready to hunt for a problem frequency.",
          { { P::gainParamId (0), 0.0f }, { P::gainParamId (1), 0.0f },
            { P::gainParamId (2), 0.0f }, { P::gainParamId (3), 0.0f } } },
        { "Sweep a narrow, boosted band through the source to find what rings or "
          "honks - here, Bell 1 at 400 Hz with a high Q and a strong boost.",
          { { P::freqParamId (1), 400.0f }, { P::gainParamId (1), 8.0f }, { P::qParamId (1), 8.0f } } },
        { "Once you've found the loudest, most resonant spot, flip that same "
          "boost into a cut instead.",
          { { P::gainParamId (1), -4.0f } } },
        { "Widen the Q a little so the fix sits naturally instead of sounding "
          "surgical.",
          { { P::qParamId (1), 1.5f } } },
        { "Compare: step back to hear the search-boost again, or close the "
          "lesson to keep the cut.",
          {} }
    });
}

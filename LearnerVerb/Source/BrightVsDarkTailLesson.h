#pragma once

#include "../../shared/MicroLesson.h"
#include "PluginProcessor.h"

// Second LearnerVerb lesson (see decisions/017): frequency-dependent decay
// from docs/knowledge_base.md's "Реверберация и пространство" section -
// high frequencies in a real space die out faster than low ones, and
// Damping is what lets this reverb imitate that independently of overall
// Decay length. A different focus than buildVocalSpaceLesson()'s
// straight predelay/decay walkthrough.
inline MicroLesson buildBrightVsDarkTailLesson()
{
    using P = LearnerVerbProcessor;

    return MicroLesson ("Bright vs. Dark Tail", {
        { "Dry signal, no reverb.",
          { { P::dryWetParamId, 0.0f } } },
        { "Add a Hall reverb with low damping - the tail stays bright and "
          "metallic all the way through its decay.",
          { { P::typeParamId, 1.0f }, { P::decayParamId, 2.0f }, { P::dryWetParamId, 30.0f }, { P::dampingParamId, 10.0f } } },
        { "Now raise damping a lot, same decay time - the top end dies out "
          "much faster than the low end.",
          { { P::dampingParamId, 85.0f } } },
        { "In a real room, high frequencies almost always fade faster than "
          "low ones - damping is what lets a reverb imitate that "
          "independently of the overall decay length.",
          {} },
        { "Compare: step back to hear the bright tail again, or close the "
          "lesson to keep the darker one.",
          {} }
    });
}

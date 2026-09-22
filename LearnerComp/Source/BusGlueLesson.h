#pragma once

#include "shared/learning/MicroLesson.h"
#include "PluginProcessor.h"

// Second LearnerComp lesson (see decisions/017): the "glue" bus-compression
// recipe from docs/knowledge_base.md's "Компрессия и динамическая
// обработка" section - moderate ratio, slow attack, only a couple of dB of
// gain reduction - a different technique than buildVocalCompressionLesson()'s
// per-parameter walkthrough.
inline MicroLesson buildBusGlueLesson()
{
    using P = LearnerCompProcessor;

    return MicroLesson ("Bus Glue Compression", {
        { "Original signal, no compression.",
          { { P::bypassParamId, 1.0f }, { P::thresholdParamId, 0.0f }, { P::ratioParamId, 1.0f },
            { P::attackParamId, 10.0f }, { P::releaseParamId, 150.0f }, { P::kneeParamId, 0.0f },
            { P::makeupParamId, 0.0f }, { P::dryWetParamId, 100.0f } } },
        { "A gentle 2:1 ratio with a high threshold - just brushing the "
          "loudest peaks, not squashing everything.",
          { { P::bypassParamId, 0.0f }, { P::thresholdParamId, -8.0f }, { P::ratioParamId, 2.0f } } },
        { "A slow 30 ms attack lets transients through untouched - the "
          "classic glue-compression setting.",
          { { P::attackParamId, 30.0f } } },
        { "A soft knee blends the compression in gradually instead of "
          "switching on abruptly at the threshold.",
          { { P::kneeParamId, 12.0f } } },
        { "The goal here is only a couple of dB of gain reduction, not ten - "
          "watch the meter rather than chasing a bigger number.",
          {} },
        { "Makeup Gain +2 dB brings the level back without overdoing it.",
          { { P::makeupParamId, 2.0f } } }
    });
}

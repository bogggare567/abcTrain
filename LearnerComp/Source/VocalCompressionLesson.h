#pragma once

#include "../../shared/MicroLesson.h"
#include "PluginProcessor.h"

// Lesson content lives per-plugin, not in shared/ - see ADR 005.
inline MicroLesson buildVocalCompressionLesson()
{
    using P = LearnerCompProcessor;

    return MicroLesson ("Vocal Compression", {
        { "Original signal, no compression.",
          { { P::bypassParamId, 1.0f }, { P::thresholdParamId, 0.0f }, { P::ratioParamId, 1.0f },
            { P::attackParamId, 10.0f }, { P::releaseParamId, 120.0f }, { P::kneeParamId, 0.0f },
            { P::makeupParamId, 0.0f }, { P::dryWetParamId, 100.0f } } },
        { "Set threshold to -18 dB - the compressor kicks in on louder peaks.",
          { { P::bypassParamId, 0.0f }, { P::thresholdParamId, -18.0f } } },
        { "Ratio 3:1 - gentle compression.",
          { { P::ratioParamId, 3.0f } } },
        { "Attack 8 ms - quick enough to catch the loud syllables, slow enough "
          "that the consonant at the front of each word keeps its edge.",
          { { P::attackParamId, 8.0f } } },
        { "Release 150 ms - a smooth recovery back to unity gain.",
          { { P::releaseParamId, 150.0f } } },
        { "Add a 6 dB knee - the same ratio now eases in gradually around "
          "the threshold instead of switching on abruptly.",
          { { P::kneeParamId, 6.0f } } },
        { "Makeup Gain +4 dB - restore the loudness the compressor took away.",
          { { P::makeupParamId, 4.0f } } }
    });
}

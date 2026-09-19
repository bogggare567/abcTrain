#pragma once

#include "../../shared/MicroLesson.h"
#include "EQSetup.h"

// The boost-and-sweep technique, and why the fix is wider than the search.
inline MicroLesson buildFindResonanceLesson()
{
    using T = EQCoefficients::BandType;
    using EQSetup::bands;

    return MicroLesson ("Find & Fix a Resonance", {
        { "Flat, one band ready to hunt with.",
          bands ({ { T::bell, 1000.0f, 0.0f, 1.0f } }) },
        { "Search: a narrow bell, Q 8, boosted +8 dB, at 400 Hz. Sweeping a "
          "boost like this through a source makes whatever rings or honks "
          "jump out - the boost exaggerates the problem so you can find it.",
          bands ({ { T::bell, 400.0f, 8.0f, 8.0f } }) },
        { "Found it. Flip the same band into a cut, -4 dB. The search boost "
          "is never the fix; it only showed you where to look.",
          bands ({ { T::bell, 400.0f, -4.0f, 8.0f } }) },
        { "Now widen it to Q 1.5. A cut as narrow as the search sounds "
          "surgical and phasey; a slightly wider one sits in the sound.",
          bands ({ { T::bell, 400.0f, -4.0f, 1.5f } }) },
        { "Compare: step back to hear the search boost again, or finish.",
          {} }
    });
}

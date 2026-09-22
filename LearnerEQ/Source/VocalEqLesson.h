#pragma once

#include "shared/learning/MicroLesson.h"
#include "EQSetup.h"

// Four moves on a vocal, each band built explicitly (see EQSetup.h).
inline MicroLesson buildVocalEqLesson()
{
    using T = EQCoefficients::BandType;
    using EQSetup::bands;

    return MicroLesson ("Vocal EQ Basics", {
        { "The untreated sound. One band, flat - nothing is being done yet.",
          bands ({ { T::bell, 1000.0f, 0.0f } }) },
        { "Low-end warmth: a low shelf at 120 Hz, +1.5 dB. A shelf lifts "
          "everything below its corner, not one spot - which is why it reads "
          "as weight rather than as a tone.",
          bands ({ { T::lowShelf, 120.0f, 1.5f } }) },
        { "Presence: a bell at 3 kHz, +3 dB. This is where consonants and "
          "the edge of the voice live; a little brings the words forward.",
          bands ({ { T::lowShelf, 120.0f, 1.5f }, { T::bell, 3000.0f, 3.0f, 1.0f } }) },
        { "Mud: a bell at 250 Hz, -3 dB. Most boxy, cardboard-sounding vocals "
          "have too much here, and cutting it lets the presence boost do less.",
          bands ({ { T::lowShelf, 120.0f, 1.5f }, { T::bell, 3000.0f, 3.0f, 1.0f },
                   { T::bell, 250.0f, -3.0f, 1.2f } }) },
        { "Air: a high shelf at 10 kHz, +2 dB. Openness above the voice itself "
          "- too much and it turns into hiss rather than brightness.",
          bands ({ { T::lowShelf, 120.0f, 1.5f }, { T::bell, 3000.0f, 3.0f, 1.0f },
                   { T::bell, 250.0f, -3.0f, 1.2f }, { T::highShelf, 10000.0f, 2.0f } }) },
        { "Compare: step back to hear it without, or finish to put the plugin "
          "back the way it was.",
          {} }
    });
}

#pragma once

#include "shared/learning/MicroLesson.h"
#include "EQSetup.h"

// Four moves on a vocal, each band built explicitly (see EQSetup.h).
//
// Cuts first, boosts after (2026-10-02, the owner's review against the
// books - Owsinski, Izhaki ch. 14): what is in the way comes out before
// anything is added, so the boosts that follow can be smaller. The old
// order opened with a low-shelf boost, the habit the course teaches out.
inline MicroLesson buildVocalEqLesson()
{
    using T = EQCoefficients::BandType;
    using EQSetup::bands;

    return MicroLesson ("Vocal EQ Basics", {
        { "The untreated sound. One band, flat - nothing is being done yet.",
          bands ({ { T::bell, 1000.0f, 0.0f } }) },
        { "Rumble out: a high-pass at 90 Hz. Below the voice there is only "
          "stage noise, breath and the proximity boost of the mic - taking it "
          "away cleans the low end without thinning the voice.",
          bands ({ { T::highPass, 90.0f, 0.0f, 0.7f } }) },
        { "Mud: a bell at 250 Hz, -3 dB. Most boxy, cardboard-sounding vocals "
          "have too much here, and cutting it lets the presence boost do less.",
          bands ({ { T::highPass, 90.0f, 0.0f, 0.7f }, { T::bell, 250.0f, -3.0f, 1.2f } }) },
        { "Presence: a bell at 3 kHz, +3 dB. This is where consonants and "
          "the edge of the voice live; a little brings the words forward.",
          bands ({ { T::highPass, 90.0f, 0.0f, 0.7f }, { T::bell, 250.0f, -3.0f, 1.2f },
                   { T::bell, 3000.0f, 3.0f, 1.0f } }) },
        { "Air: a high shelf at 10 kHz, +2 dB. Openness above the voice itself "
          "- too much and it turns into hiss rather than brightness.",
          bands ({ { T::highPass, 90.0f, 0.0f, 0.7f }, { T::bell, 250.0f, -3.0f, 1.2f },
                   { T::bell, 3000.0f, 3.0f, 1.0f }, { T::highShelf, 10000.0f, 2.0f } }) },
        { "Compare: step back to hear it without, or finish to put the plugin "
          "back the way it was.",
          {} }
    });
}

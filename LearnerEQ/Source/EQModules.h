#pragma once

#include "../../shared/TrainingModule.h"
#include "EQSetup.h"
#include <vector>
#include "VocalEqLesson.h"
#include "FindResonanceLesson.h"
#include "HighPassLesson.h"

// Learner EQ's modules (ADR 037). It had none - by an old decision that a
// graphical EQ's answer is on the curve, which is true and is exactly why a
// check has to cover the curve and ask for the answer by ear. Each one is
// the same question the trainer's Guess the Band asks, answered with this
// plugin's own knob: the check is always on band 1, and the editor selects
// it when a module opens.
namespace EQModules
{
    inline std::vector<TrainingModule::Definition> all()
    {
        using namespace TrainingModule;
        using T = EQCoefficients::BandType;
        using EQSetup::bands;
        using P = LearnerEQProcessor;

        return {
            {
                "eq.frequency",
                "Frequency",
                "Where a boost is. The one EQ skill everything else rests on.",
                "Sweep the frequency knob until the boost you hear sits where you "
                "think it should, then listen to it move above and below.",
                {
                    { "A +9 dB bell at 250 Hz. Warm, thick - it is in the body of the sound.",
                      bands ({ { T::bell, 250.0f, 9.0f, 1.4f } }) },
                    { "The same bell at 1 kHz. Nasal, honky - this is the mids most "
                      "boosts end up in by accident.",
                      bands ({ { T::bell, 1000.0f, 9.0f, 1.4f } }) },
                    { "And at 5 kHz. Edge, presence, and the start of harshness.",
                      bands ({ { T::bell, 5000.0f, 9.0f, 1.4f } }) }
                },
                {
                    P::freqParamId (0), Bed::pinkNoise,
                    100.0f, 10000.0f, true, 0.0f,
                    Unit::octaves, 1.0f, 0.2f,
                    {}, " Hz", 1.0f
                }
            },
            {
                "eq.gain",
                "Gain",
                "How much - and which way. A 3 dB cut and a 3 dB boost are different sounds.",
                "Set the gain until the change is as big as you want it, then halve "
                "it and listen again. Most useful moves are smaller than they first seem.",
                {
                    { "A bell at 800 Hz, +6 dB. Obvious.",
                      bands ({ { T::bell, 800.0f, 6.0f, 1.0f } }) },
                    { "+2 dB. Still there, if you switch back and forth - this is the "
                      "size of most real moves.",
                      bands ({ { T::bell, 800.0f, 2.0f, 1.0f } }) },
                    { "-6 dB. A cut takes something away rather than adding a colour; "
                      "it is often heard as the sound getting clearer, not quieter.",
                      bands ({ { T::bell, 800.0f, -6.0f, 1.0f } }) }
                },
                {
                    P::gainParamId (0), Bed::pinkNoise,
                    -12.0f, 12.0f, false, 0.5f,
                    Unit::decibels, 4.0f, 1.0f,
                    {}, " dB", 1.0f
                }
            },
            {
                "eq.q",
                "Width (Q)",
                "How much of the spectrum a band takes with it.",
                "Narrow the band until it stops sounding like a tone change and starts "
                "sounding like one ringing note. That point is higher than you think.",
                {
                    { "A wide bell, Q 0.5, +8 dB at 1 kHz. It colours everything "
                      "around it - a tilt more than a spot.",
                      bands ({ { T::bell, 1000.0f, 8.0f, 0.5f } }) },
                    { "Q 8. The same boost, now a single ringing pitch.",
                      bands ({ { T::bell, 1000.0f, 8.0f, 8.0f } }) }
                },
                {
                    P::qParamId (0), Bed::pinkNoise,
                    0.5f, 8.0f, true, 0.0f,
                    Unit::proportion, 0.9f, 0.3f,
                    {}, "", 1.0f
                }
            },
            {
                "eq.highpass",
                "High-pass",
                "Where the bottom stops. The most used filter in a mix, and the most overdone.",
                "Raise the cutoff until the bass note starts to lose its weight, then "
                "back it off until it comes back. That is the edge.",
                {
                    { "A high-pass at 30 Hz: nothing audible is gone, only rumble.",
                      bands ({ { T::highPass, 30.0f, 0.0f, 0.7f } }) },
                    { "At 120 Hz. The note is still there, its weight is not.",
                      bands ({ { T::highPass, 120.0f, 0.0f, 0.7f } }) },
                    { "At 300 Hz. Thin - this is a telephone, not a bass.",
                      bands ({ { T::highPass, 300.0f, 0.0f, 0.7f } }) }
                },
                {
                    P::freqParamId (0), Bed::bassNote,
                    30.0f, 400.0f, true, 0.0f,
                    Unit::octaves, 1.0f, 0.25f,
                    {}, " Hz", 1.0f
                }
            }
        };
    }

    // The multi-knob walkthroughs, as checkless modules (ADR 037).
    inline std::vector<TrainingModule::Definition> walkthroughs()
    {
        using TrainingModule::Bed;

        return {
            TrainingModule::walkthrough ("eq.walk.vocal", "Four moves on a voice, one at a time.", Bed::chord, buildVocalEqLesson()),
            TrainingModule::walkthrough ("eq.walk.resonance", "Boost to find it, cut to fix it.", Bed::pinkNoise, buildFindResonanceLesson()),
            TrainingModule::walkthrough ("eq.walk.highpass", "What a high-pass takes with it.", Bed::bassNote, buildHighPassLesson()),
            TrainingModule::walkthrough ("eq.walk.lowpass", "Where the top end stops being useful.", Bed::brightHit, buildLowPassLesson())
        };
    }
}

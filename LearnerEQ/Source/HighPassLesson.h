#pragma once

#include "../../shared/MicroLesson.h"
#include "PluginProcessor.h"

// Why a high-pass is not free, and why "brickwall" is not a compliment.
//
// This is the lesson the plugin could not teach before, because it had no
// pass filters at all - four fixed bands, two of them shelves. It is also
// the single most-used and least-understood move in a mix: nearly everyone
// high-passes everything, and almost nobody has heard what the filter does
// on the way past.
//
// The content is general, widely-taught practice - the phase behaviour of
// minimum-phase filters, the resonance of a high-Q pole pair, the cost of
// a steep slope - written here in this project's own words, the same rule
// as every other guide text (see ADR 010: no book is quoted, and the
// bibliography names titles only).
//
// Everything below is *audible on the plugin's own display*, which is the
// point: each step leaves a curve you can see and a sound you can hear,
// rather than a claim you have to take on trust.
inline MicroLesson buildHighPassLesson()
{
    using P = LearnerEQProcessor;
    using T = EQCoefficients::BandType;

    const auto type = [] (int band, T value)
    {
        return std::pair<juce::String, float> { P::typeParamId (band), (float) (int) value };
    };

    return MicroLesson ("High-pass: what it costs", {
        { "Flat, one band, doing nothing. Listen to the low end as it is - "
          "you need the before, or the after means nothing.",
          { { P::onParamId (0), 1.0f }, { P::gainParamId (0), 0.0f },
            type (0, T::bell), { P::freqParamId (0), 1000.0f },
            { P::onParamId (1), 0.0f }, { P::onParamId (2), 0.0f } } },

        { "A gentle high-pass at 40 Hz, Q 0.7. Below this sits rumble, "
          "handle noise and room energy that costs headroom and gives "
          "nothing back. On most sources this is the safest EQ move there "
          "is - and on a kick or a bass it is already too high.",
          { type (0, T::highPass), { P::freqParamId (0), 40.0f },
            { P::qParamId (0), 0.7f } } },

        { "Now drag it up to 150 Hz. Somewhere on the way, the source stops "
          "sounding cleaner and starts sounding thin. That point is not a "
          "number you can be told - it moves with the instrument, the "
          "arrangement and the speaker you are on.",
          { { P::freqParamId (0), 150.0f } } },

        { "Q 4 at the same frequency. The curve now lifts just above the "
          "cutoff before it falls. That bump is real: a resonant pole pair "
          "peaks near its corner, and a high-Q high-pass adds energy "
          "exactly where you were trying to remove it. It can be useful - "
          "it is also why a 'tighter' filter can make a bass boomier.",
          { { P::qParamId (0), 4.0f } } },

        { "Back to Q 0.7, and one more pass filter stacked on top: two in "
          "series is a steeper slope. Steeper removes more, faster - and "
          "every pole you add turns the phase further near the corner. A "
          "minimum-phase filter cannot change amplitude without changing "
          "phase; the two are the same fact seen twice.",
          { { P::qParamId (0), 0.7f },
            { P::onParamId (1), 1.0f }, type (1, T::highPass),
            { P::freqParamId (1), 150.0f }, { P::qParamId (1), 0.7f } } },

        { "Why that matters: phase rotation is inaudible on a solo'd track "
          "and very audible when two tracks are summed. High-pass a kick "
          "and a bass at the same corner with steep filters and the region "
          "just above it can partially cancel - the fault is not in either "
          "track, it is in the sum. Bypass and compare on the mix, not on "
          "the source.",
          {} },

        { "This is why 'brickwall' is a description, not a goal. An "
          "extremely steep filter needs many poles, and pays for them in "
          "phase smear and pre-ringing around the corner. If a gentle slope "
          "gets you there, it is the better filter - reach for steep when "
          "the problem is genuinely narrow and genuinely in the way.",
          { { P::onParamId (1), 0.0f }, { P::freqParamId (0), 40.0f } } },

        { "Left where it started: one gentle high-pass at 40 Hz. Bypass to "
          "check that it is still an improvement - the move most worth "
          "making is the one you can still hear the point of.",
          {} }
    });
}

// The mirror image, and a shorter lesson: what a low-pass is actually for.
inline MicroLesson buildLowPassLesson()
{
    using P = LearnerEQProcessor;
    using T = EQCoefficients::BandType;

    const auto type = [] (int band, T value)
    {
        return std::pair<juce::String, float> { P::typeParamId (band), (float) (int) value };
    };

    return MicroLesson ("Low-pass and the top end", {
        { "Flat again. Listen to the top: cymbals, breath, the noise floor "
          "of whatever this was recorded on.",
          { { P::onParamId (0), 1.0f }, type (0, T::bell),
            { P::gainParamId (0), 0.0f }, { P::freqParamId (0), 1000.0f },
            { P::onParamId (1), 0.0f } } },

        { "A low-pass at 16 kHz. Almost nothing is missing - but hiss, "
          "cymbal wash and converter noise from several tracks add up, and "
          "removing what an instrument never produced costs it nothing.",
          { type (0, T::lowPass), { P::freqParamId (0), 16000.0f },
            { P::qParamId (0), 0.7f } } },

        { "Down to 6 kHz. Now it is an effect rather than housekeeping - "
          "the sound moves backwards and gets darker. That is the same "
          "cue distance gives you in a real room: air absorbs high "
          "frequencies over distance, so dull reads as far away.",
          { { P::freqParamId (0), 6000.0f } } },

        { "A high shelf can do a gentler version of the same thing. Set "
          "-4 dB from 6 kHz up: the top is quieter but still there, which "
          "is usually what you meant when you reached for the low-pass.",
          { type (0, T::highShelf), { P::freqParamId (0), 6000.0f },
            { P::gainParamId (0), -4.0f }, { P::qParamId (0), 0.7f } } },

        { "Both are correct tools. The pass filter says 'nothing above "
          "here'; the shelf says 'less above here'. Reaching for the first "
          "when you meant the second is how a mix ends up dull instead of "
          "clean.",
          {} }
    });
}

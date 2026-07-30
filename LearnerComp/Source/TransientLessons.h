#pragma once

#include "../../shared/MicroLesson.h"
#include "PluginProcessor.h"

// The two compressor lessons that are about *time* rather than about
// amount - and the two knobs people set by habit rather than by ear.
//
// The existing pair (buildVocalCompressionLesson, buildBusGlueLesson) walk
// two workflows: here is a vocal chain, here is a bus chain. Useful, and
// neither one explains why a knob does what it does. These do, in the
// plugin's own words, from general widely-taught practice rather than from
// any particular book (ADR 010).
//
// Every step is audible on this plugin's own gain-reduction meter, which
// is the reason they belong here rather than in a document: you can watch
// the envelope do the thing the sentence describes.

inline MicroLesson buildAttackLesson()
{
    using P = LearnerCompProcessor;

    return MicroLesson ("Attack: why fast kills the punch", {
        { "Compressor out of the way: ratio 1:1, so nothing is happening. "
          "Listen to the front of each hit - that first few milliseconds is "
          "the transient, and it is most of what makes a drum sound like a "
          "drum rather than like a tone.",
          { { P::ratioParamId, 1.0f }, { P::thresholdParamId, 0.0f },
            { P::makeupParamId, 0.0f }, { P::dryWetParamId, 1.0f },
            { P::kneeParamId, 6.0f } } },

        { "Now 4:1 at -18 dB with a 1 ms attack. The compressor catches the "
          "transient itself, so the loudest part of every hit is the part "
          "that gets flattened. Watch the meter: it slams down on each hit. "
          "The result is even - and blunt.",
          { { P::ratioParamId, 4.0f }, { P::thresholdParamId, -18.0f },
            { P::attackParamId, 1.0f }, { P::releaseParamId, 120.0f } } },

        { "Same settings, attack 30 ms. Now the transient gets through "
          "before the gain reduction arrives, and only the body behind it "
          "is compressed. Louder attack against quieter body reads as *more* "
          "punch - from a compressor, which is supposed to reduce dynamics.",
          { { P::attackParamId, 30.0f } } },

        { "That is the counterintuitive part worth keeping: a slow attack "
          "makes things hit harder, a fast attack makes them sit still. "
          "Neither is correct - they answer different questions. \"Control "
          "the level\" wants fast; \"make it punch\" wants slow.",
          {} },

        { "Attack is relative to the source, not an absolute. 30 ms on a "
          "snare lets the crack through; 30 ms on a sustained pad is "
          "instant, because a pad has no transient to miss. This is why a "
          "preset that worked on drums does nothing recognisable on a synth "
          "- the number moved into a different envelope.",
          {} },

        { "One caution before you go fast: at very short attack times a "
          "compressor starts tracking individual *cycles* of low frequency "
          "rather than the envelope over them, and that is distortion, not "
          "level control. On bass it is audible as a growl that was not "
          "there before.",
          { { P::attackParamId, 0.5f }, { P::ratioParamId, 8.0f } } },

        { "Back to something usable: 4:1, 20 ms, and compare against bypass. "
          "The question is never \"is it compressed\" - it is whether the "
          "thing you wanted to hear got louder relative to the thing you "
          "did not.",
          { { P::ratioParamId, 4.0f }, { P::attackParamId, 20.0f },
            { P::makeupParamId, 3.0f } } }
    });
}

inline MicroLesson buildReleaseLesson()
{
    using P = LearnerCompProcessor;

    return MicroLesson ("Release, and where pumping comes from", {
        { "4:1 at -18 dB, attack 20 ms, release very long - 800 ms. The "
          "compressor grabs and then holds on. Between hits it has still "
          "not let go, so quiet passages stay quiet and the track loses its "
          "own shape.",
          { { P::ratioParamId, 4.0f }, { P::thresholdParamId, -18.0f },
            { P::attackParamId, 20.0f }, { P::releaseParamId, 800.0f },
            { P::kneeParamId, 6.0f }, { P::makeupParamId, 3.0f },
            { P::dryWetParamId, 1.0f } } },

        { "Now 30 ms. It releases fully between every hit, which means the "
          "gain is moving constantly - up in the gaps, down on the hits. "
          "That audible breathing in and out is pumping. On a drum bus it "
          "can be the effect you want; under a vocal it is a distraction "
          "that never resolves.",
          { { P::releaseParamId, 30.0f } } },

        { "Very fast release has the same problem fast attack does, from "
          "the other end: below roughly one cycle of the lowest frequency "
          "present, the envelope follows the waveform instead of the "
          "loudness, and you get harmonic distortion. Low frequencies are "
          "long - 50 Hz is 20 ms per cycle - which is why bass needs slower "
          "release than a hi-hat does.",
          { { P::releaseParamId, 10.0f } } },

        { "A useful starting point: let it recover roughly in time with the "
          "music. Around 150 ms sits near an eighth note at 100 BPM, so the "
          "gain moves with the groove rather than against it. Then adjust "
          "by ear - the tempo is a starting place, not a rule.",
          { { P::releaseParamId, 150.0f } } },

        { "The last honest thing about both knobs: aim for a few dB of gain "
          "reduction and judge the result by bypassing, not by the meter. A "
          "meter shows how hard the compressor is working, which is not the "
          "same question as whether the track got better.",
          { { P::thresholdParamId, -14.0f }, { P::makeupParamId, 2.0f } } }
    });
}

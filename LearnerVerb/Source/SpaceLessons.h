#pragma once

#include "../../shared/MicroLesson.h"
#include "PluginProcessor.h"

// The two reverb lessons about *why*, alongside the two existing ones
// (buildVocalSpaceLesson, buildBrightVsDarkTailLesson) about what to dial.
//
// General, widely-taught practice written in this project's own words, the
// same rule as every other guide text here (ADR 010). Both are audible on
// the plugin's own display and meters, which is why they belong in it.

inline MicroLesson buildPreDelayLesson()
{
    using P = LearnerVerbProcessor;

    return MicroLesson ("Pre-delay: staying in front of the room", {
        { "Completely dry. This is the reference - a source with no space "
          "around it at all, which is also what a close microphone in a "
          "treated room gives you.",
          { { P::dryWetParamId, 0.0f }, { P::preDelayParamId, 0.0f },
            { P::decayParamId, 1.8f }, { P::sizeParamId, 0.5f },
            { P::dampingParamId, 0.4f }, { P::widthParamId, 1.0f } } },

        { "A hall at 30% wet, pre-delay 0. The reverb starts at the same "
          "instant the sound does, so the two are glued together. It sounds "
          "distant, and the words get harder to follow - the tail is "
          "covering the very consonants that make them legible.",
          { { P::typeParamId, 1.0f }, { P::dryWetParamId, 0.3f },
            { P::preDelayParamId, 0.0f } } },

        { "Same reverb, pre-delay 40 ms. The dry sound now arrives alone "
          "and the room follows it. Nothing about the tail changed - only "
          "when it starts - and the source has moved forward, stayed "
          "intelligible, and kept its space.",
          { { P::preDelayParamId, 40.0f } } },

        { "Why that works: the ear locates a sound by what reaches it "
          "*first*, and for a few tens of milliseconds afterwards it treats "
          "reflections as part of the same event rather than as a second "
          "one. Pre-delay puts the room inside that window, so you hear one "
          "sound in a space rather than two sounds.",
          {} },

        { "There is a limit. Push it to 120 ms and the reflection stops "
          "being part of the sound and becomes an echo - a separate event, "
          "arriving late. Somewhere between those two it stops being space "
          "and starts being rhythm.",
          { { P::preDelayParamId, 120.0f } } },

        { "A practical trick: pre-delay in time with the track keeps the "
          "room from smearing across the grid. Back to 40 ms - and note "
          "that on a fast, dense arrangement, less wet with more pre-delay "
          "usually beats more wet with none.",
          { { P::preDelayParamId, 40.0f }, { P::dryWetParamId, 0.25f } } }
    });
}

inline MicroLesson buildSizeAndDampingLesson()
{
    using P = LearnerVerbProcessor;

    return MicroLesson ("Three ways to say bigger", {
        { "A small, dry-ish room to start from. Decay, Size and Damping all "
          "make a space sound larger or smaller, and they are not "
          "interchangeable - this walks what each one actually changes.",
          { { P::typeParamId, 0.0f }, { P::decayParamId, 0.8f },
            { P::sizeParamId, 0.3f }, { P::dampingParamId, 0.5f },
            { P::preDelayParamId, 20.0f }, { P::dryWetParamId, 0.3f },
            { P::widthParamId, 1.0f } } },

        { "Decay to 3.5 s, nothing else touched. The room did not get "
          "bigger - it got *more reflective*. This is the difference "
          "between a hall and a tiled bathroom: how long energy survives, "
          "not how far it travels.",
          { { P::decayParamId, 3.5f } } },

        { "Decay back down, Size up instead. Now the early reflections "
          "arrive further apart, which is the cue that actually says "
          "\"large room\". A short decay in a big space is a real thing - "
          "a well-treated concert hall behaves like that.",
          { { P::decayParamId, 1.2f }, { P::sizeParamId, 0.9f } } },

        { "Damping is the third one, and it is the most physical. Real "
          "surfaces absorb high frequencies faster than low ones, so a real "
          "tail gets darker as it decays. At damping 0 the tail keeps its "
          "top end all the way out, which no room does - it reads as "
          "metallic and artificial.",
          { { P::dampingParamId, 0.0f } } },

        { "Damping up to 75%. Same decay time, but the high end dies away "
          "first, and the space suddenly has surfaces in it - curtains, "
          "wood, people. This is usually the knob that makes a reverb stop "
          "sounding like a plugin.",
          { { P::dampingParamId, 0.75f } } },

        { "The last one is not on this plugin, and is worth saying anyway: "
          "reverb belongs on a send, not on every insert. One room that "
          "several tracks share sounds like a place; a different room per "
          "track sounds like several recordings edited together - and costs "
          "several times the CPU to get there.",
          { { P::sizeParamId, 0.6f }, { P::decayParamId, 1.8f },
            { P::dryWetParamId, 0.28f } } }
    });
}

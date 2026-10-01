#pragma once

#include "shared/learning/MicroLesson.h"
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
          { { P::bypassParamId, 0.0f }, { P::typeParamId, 1.0f },
            { P::dryWetParamId, 0.0f }, { P::preDelayParamId, 0.0f },
            { P::decayParamId, 1.8f }, { P::sizeParamId, 50.0f },
            { P::dampingParamId, 40.0f }, { P::widthParamId, 100.0f } } },

        { "A hall at 30% wet, pre-delay 0. The reverb starts at the same "
          "instant the sound does, so the two are glued together. It sounds "
          "distant, and the words get harder to follow - the tail is "
          "covering the very consonants that make them legible.",
          { { P::typeParamId, 1.0f }, { P::dryWetParamId, 30.0f },
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
          { { P::preDelayParamId, 40.0f }, { P::dryWetParamId, 25.0f } } }
    });
}

inline MicroLesson buildSizeAndDampingLesson()
{
    using P = LearnerVerbProcessor;

    return MicroLesson ("Three ways to say bigger", {
        { "A small, dry-ish room to start from. Decay, Size and Damping all "
          "make a space sound larger or smaller, and they are not "
          "interchangeable - this walks what each one actually changes.",
          { { P::bypassParamId, 0.0f }, { P::typeParamId, 0.0f }, { P::decayParamId, 0.8f },
            { P::sizeParamId, 30.0f }, { P::dampingParamId, 50.0f },
            { P::preDelayParamId, 20.0f }, { P::dryWetParamId, 30.0f },
            { P::widthParamId, 100.0f } } },

        { "Decay to 3.5 s, nothing else touched. The room did not get "
          "bigger - it got *more reflective*. This is the difference "
          "between a hall and a tiled bathroom: how long energy survives, "
          "not how far it travels.",
          { { P::decayParamId, 3.5f } } },

        { "Decay back down, Size up instead. Now the early reflections "
          "arrive further apart, which is the cue that actually says "
          "\"large room\". A short decay in a big space is a real thing - "
          "a well-treated concert hall behaves like that.",
          { { P::decayParamId, 1.2f }, { P::sizeParamId, 90.0f } } },

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
          { { P::dampingParamId, 75.0f } } },

        { "One more, from routing rather than a knob: Routing is now SEND. "
          "Reverb belongs on a send, not on every insert - one room that "
          "several tracks share sounds like a place; a different room per "
          "track sounds like several recordings edited together. Here Mix "
          "has become the return fader; the dry track is untouched.",
          { { P::sizeParamId, 60.0f }, { P::decayParamId, 1.8f },
            { P::dryWetParamId, 28.0f }, { P::routingParamId, (float) P::send } } }
    });
}

// RT60 (2026-10, at the owner's request): what the number under Decay
// means. Sabine's definition and formula (W. C. Sabine, 1900; F. Alton
// Everest, Master Handbook of Acoustics, ch. on reverberation), typical
// values of real spaces, why the measured figure differs from the knob,
// and the working rule of fitting the tail to the tempo.
inline MicroLesson buildRt60Lesson()
{
    using P = LearnerVerbProcessor;

    return MicroLesson ("What RT60 means", {
        { "RT60 is the time it takes a sound to fall by 60 dB after the source "
          "stops - from loud to as good as gone. Here: a hall, Decay 2 s. "
          "Listen to the tail after each hit and watch the echogram on the right.",
          { { P::bypassParamId, 0.0f }, { P::routingParamId, (float) P::insert }, { P::typeParamId, 1.0f },
            { P::decayParamId, 2.0f }, { P::sizeParamId, 70.0f }, { P::dampingParamId, 40.0f },
            { P::preDelayParamId, 20.0f }, { P::dryWetParamId, 35.0f }, { P::widthParamId, 100.0f } } },

        { "Why 60 dB: a loud source is about 100 dB, a quiet room about 40 dB - "
          "60 dB down is where the tail drowns in the room's own noise. "
          "Sabine's formula: RT60 = 0.161 * V / A. More volume (V), longer tail; "
          "more absorption (A: curtains, people, sofas), shorter.",
          {} },

        { "Real spaces, for reference: a vocal booth 0.2-0.3 s, a living room "
          "0.4-0.6 s, a control room about 0.3 s, a concert hall 1.8-2.2 s, a "
          "cathedral 4-8 s. Now 0.5 s, a room - the space you mostly feel "
          "rather than hear.",
          { { P::typeParamId, 0.0f }, { P::decayParamId, 0.5f }, { P::sizeParamId, 35.0f } } },

        { "Look under Decay: \"RT60 measured\" is not always the knob. The "
          "plugin measures its own tail the way acousticians do - the slope "
          "of the decay - and Damping makes the top die sooner, so the "
          "measured figure comes out shorter. The knob is a wish; RT60 is what "
          "the room actually does.",
          { { P::typeParamId, 1.0f }, { P::decayParamId, 2.5f }, { P::dampingParamId, 85.0f } } },

        { "The working rule: fit the tail to the tempo. At 120 BPM a beat is "
          "0.5 s and a bar is 2 s - a tail that ends before the next hit keeps "
          "the drums apart, a tail of a bar or more turns them into a wash. "
          "On a voice, let it end before the next phrase.",
          { { P::dampingParamId, 40.0f }, { P::decayParamId, 0.5f } } },

        { "Compare: step back through the hall and the room, or finish to "
          "keep this setting.",
          {} }
    });
}

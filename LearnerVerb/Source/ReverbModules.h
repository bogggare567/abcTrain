#pragma once

#include "../../shared/TrainingModule.h"
#include <vector>

// One module per control on this reverb. Same shape and same reasoning as
// LearnerComp/Source/CompressorModules.h - content stays with the plugin
// whose parameter IDs it names.
namespace ReverbModules
{
    inline std::vector<TrainingModule::Definition> all()
    {
        using namespace TrainingModule;

        return {
            {
                "verb.type",
                "Type",
                "Which kind of space or device the tail is pretending to be.",
                "Listen to the same hit through each of the four and find the one "
                "you would call metallic. Type is the one reverb decision you make "
                "with your ears before you touch a number.",
                {
                    { "Room. Short, dense, early reflections close together - it "
                      "sounds like the sound happened somewhere rather than nowhere.",
                      { { "type", 0.0f }, { "decay", 1.0f }, { "preDelay", 10.0f },
                        { "size", 35.0f }, { "damping", 40.0f }, { "dryWet", 45.0f },
                        { "width", 100.0f }, { "bypass", 0.0f } } },
                    { "Hall. The same hit in something much bigger: the reflections "
                      "arrive later and take far longer to die away.",
                      { { "type", 1.0f }, { "decay", 3.0f }, { "size", 80.0f } } },
                    { "Plate. Not a room at all - a sheet of metal. Very dense from "
                      "the first instant, with no sense of a wall anywhere.",
                      { { "type", 2.0f }, { "decay", 2.0f }, { "size", 55.0f } } },
                    { "Spring. A metal coil, and it announces itself: that boing is "
                      "the sound bouncing along the spring and back.",
                      { { "type", 3.0f }, { "decay", 1.8f } } }
                },
                {
                    "type", Bed::singleHit,
                    0.0f, 3.0f, false, 1.0f,
                    Unit::choice, 1.0f, 1.0f,
                    { "Room", "Hall", "Plate", "Spring" }, "", 1.0f
                }
            },
            {
                "verb.decay",
                "Decay",
                "How long the tail takes to fade to nothing.",
                "Set a decay that fits the gap between hits. If the tail is still "
                "going when the next hit lands, the two are fighting.",
                {
                    { "0.6 seconds. The tail is gone before you have finished "
                      "noticing it - this is ambience, not reverb you can point at.",
                      { { "type", 1.0f }, { "decay", 0.6f }, { "preDelay", 15.0f },
                        { "size", 60.0f }, { "damping", 40.0f }, { "dryWet", 45.0f },
                        { "width", 100.0f }, { "bypass", 0.0f } } },
                    { "2.5 seconds. Now it is an effect you would describe. Notice "
                      "you have not made it louder, only longer.",
                      { { "decay", 2.5f } } },
                    { "7 seconds. Long enough that the space becomes the subject and "
                      "whatever went into it stops being legible.",
                      { { "decay", 7.0f } } }
                },
                {
                    "decay", Bed::singleHit,
                    0.5f, 6.0f, true, 0.1f,
                    Unit::proportion, 0.6f, 0.2f,
                    {}, " s", 1.0f
                }
            },
            {
                "verb.preDelay",
                "Pre-delay",
                "The silence between the dry sound and the first of its reflections.",
                "Raise the pre-delay until the hit separates from its own tail. This "
                "is how you get a big space without losing the front of the sound.",
                {
                    { "0 ms. The reverb starts at the same instant as the hit, so the "
                      "two are one event and the hit loses its edge.",
                      { { "type", 1.0f }, { "decay", 2.2f }, { "preDelay", 0.0f },
                        { "size", 70.0f }, { "damping", 40.0f }, { "dryWet", 50.0f },
                        { "width", 100.0f }, { "bypass", 0.0f } } },
                    { "40 ms. The hit lands first, alone, and the space arrives just "
                      "after it. Same reverb, same amount of it, and the source is "
                      "suddenly in front instead of inside.",
                      { { "preDelay", 40.0f } } },
                    { "140 ms. Far enough that it stops reading as a room and starts "
                      "reading as a separate echo. Useful, but it is a different "
                      "effect now.",
                      { { "preDelay", 140.0f } } }
                },
                {
                    "preDelay", Bed::singleHit,
                    5.0f, 150.0f, true, 5.0f,
                    Unit::proportion, 0.7f, 0.22f,
                    {}, " ms", 1.0f
                }
            },
            {
                "verb.size",
                "Size",
                "How far apart the reflecting surfaces are.",
                "Change size without touching decay. You are listening for how "
                "*dense* the tail is, not how long - a small room with a long decay "
                "is a real and quite strange sound.",
                {
                    { "Small. Reflections arrive close together, so the tail is dense "
                      "and slightly boxy.",
                      { { "type", 0.0f }, { "decay", 1.8f }, { "preDelay", 15.0f },
                        { "size", 15.0f }, { "damping", 40.0f }, { "dryWet", 45.0f },
                        { "width", 100.0f }, { "bypass", 0.0f } } },
                    { "Large, and the decay has not moved. The reflections are now "
                      "spread out - the same length of tail with far less packed "
                      "into it.",
                      { { "size", 90.0f } } }
                },
                {
                    "size", Bed::singleHit,
                    10.0f, 95.0f, false, 5.0f,
                    Unit::rangeFraction, 0.32f, 0.11f,
                    {}, "%", 1.0f
                }
            },
            {
                "verb.damping",
                "Damping",
                "How much faster the high frequencies die than the low ones.",
                "Raise the damping until the tail goes dark without getting shorter. "
                "Every real room does this; a reverb that does not is the thing that "
                "sounds fake.",
                {
                    { "Damping at zero. The tail stays as bright at the end as it was "
                      "at the start, which no real space does.",
                      { { "type", 1.0f }, { "decay", 3.0f }, { "preDelay", 20.0f },
                        { "size", 70.0f }, { "damping", 0.0f }, { "dryWet", 50.0f },
                        { "width", 100.0f }, { "bypass", 0.0f } } },
                    { "80%. The same decay time, but the top end is gone long before "
                      "the bottom is. That is what soft surfaces do to a room, and it "
                      "is most of what makes a reverb sit behind a mix instead of on "
                      "top of it.",
                      { { "damping", 80.0f } } }
                },
                {
                    "damping", Bed::brightHit,
                    5.0f, 95.0f, false, 5.0f,
                    Unit::rangeFraction, 0.3f, 0.1f,
                    {}, "%", 1.0f
                }
            },
            {
                "verb.width",
                "Width",
                "How far across the image the tail is spread.",
                "Narrow the tail and listen to what happens to the dry sound's "
                "position. A wide reverb on a centred source is the oldest trick "
                "there is for making something feel bigger without moving it.",
                {
                    { "Full width. The tail occupies the whole image while the source "
                      "stays where it was.",
                      { { "type", 2.0f }, { "decay", 2.0f }, { "preDelay", 25.0f },
                        { "size", 60.0f }, { "damping", 45.0f }, { "dryWet", 50.0f },
                        { "width", 100.0f }, { "bypass", 0.0f } } },
                    { "30%. The tail collapses toward the middle and sits on top of "
                      "the source instead of around it - the same amount of reverb, "
                      "much more in the way.",
                      { { "width", 30.0f } } }
                },
                {
                    "width", Bed::chord,
                    20.0f, 100.0f, false, 5.0f,
                    Unit::rangeFraction, 0.32f, 0.12f,
                    {}, "%", 1.0f
                }
            },
            {
                "verb.dryWet",
                "Mix",
                "How much of the wet signal is in the output at all.",
                "Bring the mix up until you can just hear the space, then back it off "
                "slightly. Almost every reverb that sounds wrong is a good reverb "
                "with too much of it.",
                {
                    { "Fully dry. Whatever is set above this makes no difference at "
                      "all yet - the mix control is the one that decides whether any "
                      "of the others matter.",
                      { { "type", 1.0f }, { "decay", 2.4f }, { "preDelay", 30.0f },
                        { "size", 70.0f }, { "damping", 55.0f }, { "dryWet", 0.0f },
                        { "width", 100.0f }, { "bypass", 0.0f } } },
                    { "25%. Enough to place the sound somewhere, not enough to notice "
                      "as an effect. This is where most mix reverb lives.",
                      { { "dryWet", 25.0f } } },
                    { "75%. Now the space is louder than the thing in it.",
                      { { "dryWet", 75.0f } } }
                },
                {
                    "dryWet", Bed::drumLoop,
                    10.0f, 70.0f, false, 5.0f,
                    Unit::rangeFraction, 0.3f, 0.1f,
                    {}, "%", 1.0f
                }
            }
        };
    }
}

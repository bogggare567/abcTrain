#pragma once

#include "../../shared/TrainingModule.h"
#include <vector>

// One module per knob on this compressor.
//
// Content lives here rather than in shared/, for the same reason the lesson
// files and the preset tables do: it is tied to this plugin's own parameter
// IDs and ranges, and nothing about it generalises. Only the machinery is
// shared.
//
// English, like every other explanation in the three Learner plugins. See
// the note on TrainingModule::Definition for why translating the name of a
// module whose own text is untranslated would read worse than not.
namespace CompressorModules
{
    inline std::vector<TrainingModule::Definition> all()
    {
        using namespace TrainingModule;

        return {
            {
                "comp.threshold",
                "Threshold",
                "The level above which the compressor starts working at all.",
                "Bring the threshold down until the loud parts of the bass are "
                "held back but the quiet parts are still untouched. Watch the "
                "gain reduction meter: you want it moving, not pinned.",
                {
                    { "Threshold all the way up. The compressor is doing nothing - "
                      "everything is below it, so nothing is above it.",
                      { { "threshold", 0.0f }, { "ratio", 4.0f }, { "makeup", 0.0f },
                        { "attack", 10.0f }, { "release", 120.0f }, { "knee", 6.0f },
                        { "dryWet", 100.0f }, { "bypass", 0.0f } } },
                    { "At -12 dB only the peaks poke above the line, so only the "
                      "peaks get touched. The rest of the sound is untouched.",
                      { { "threshold", -12.0f } } },
                    { "At -30 dB almost everything is above the line. The whole "
                      "signal is being held down now, not just its loudest moments - "
                      "which is a different effect, not more of the same one.",
                      { { "threshold", -30.0f } } }
                },
                {
                    "threshold", Bed::bassNote,
                    -30.0f, -6.0f, false, 0.5f,
                    Unit::decibels, 5.0f, 1.5f,
                    {}, " dB", 1.0f
                }
            },
            {
                "comp.ratio",
                "Ratio",
                "How hard it pushes back once the signal is over the threshold.",
                "Set the ratio so the drums are evened out but still hit. Somewhere "
                "around 4:1 is a normal working answer; 20:1 is a different job.",
                {
                    { "Threshold set, ratio at 1:1. Over the line, nothing happens - "
                      "one decibel in, one decibel out.",
                      { { "threshold", -18.0f }, { "ratio", 1.0f }, { "attack", 10.0f },
                        { "release", 120.0f }, { "knee", 6.0f }, { "makeup", 0.0f },
                        { "dryWet", 100.0f }, { "bypass", 0.0f } } },
                    { "3:1. Three decibels over the threshold come out as one. "
                      "The hits are still hits, just less far apart.",
                      { { "ratio", 3.0f } } },
                    { "12:1. Practically a wall - the signal barely gets above the "
                      "threshold at all any more. Listen for what it costs: the "
                      "drums stop breathing.",
                      { { "ratio", 12.0f } } }
                },
                {
                    "ratio", Bed::drumLoop,
                    1.5f, 16.0f, true, 0.5f,
                    Unit::proportion, 0.55f, 0.18f,
                    {}, ":1", 1.0f
                }
            },
            {
                "comp.attack",
                "Attack",
                "How long the compressor waits before clamping down on a transient.",
                "Find the attack where the snare still cracks but the tail after it "
                "is controlled. Too fast and it flattens the hit; too slow and it "
                "misses the hit entirely.",
                {
                    { "1 ms. The compressor catches the very front of every hit, so "
                      "the crack of the snare is gone - it is being squashed before "
                      "you hear it.",
                      { { "threshold", -20.0f }, { "ratio", 6.0f }, { "attack", 1.0f },
                        { "release", 120.0f }, { "knee", 3.0f }, { "makeup", 0.0f },
                        { "dryWet", 100.0f }, { "bypass", 0.0f } } },
                    { "20 ms. Now the front of each hit gets through untouched and "
                      "only what follows it is held down. This is what people mean "
                      "by a compressor adding punch: it is not adding anything, it "
                      "is turning down everything except the punch.",
                      { { "attack", 20.0f } } },
                    { "80 ms. So slow that most of each hit is over before it reacts. "
                      "On fast material this is close to doing nothing.",
                      { { "attack", 80.0f } } }
                },
                {
                    // Milliseconds, graded as a proportion. Being 5 ms out at
                    // 3 ms and at 80 ms are not the same mistake, and a
                    // fixed-millisecond band would call them the same.
                    "attack", Bed::drumLoop,
                    1.0f, 80.0f, true, 0.5f,
                    Unit::proportion, 0.7f, 0.22f,
                    {}, " ms", 1.0f
                }
            },
            {
                "comp.release",
                "Release",
                "How long it takes to let go once the signal drops back down.",
                "Set the release so the compressor recovers between hits rather than "
                "pumping across them. The gain reduction meter should return to zero "
                "and then move again, not hover.",
                {
                    { "30 ms. It lets go almost immediately after each hit, so the "
                      "level jumps back up between them. That is the pumping people "
                      "either love or fix.",
                      { { "threshold", -20.0f }, { "ratio", 6.0f }, { "attack", 10.0f },
                        { "release", 30.0f }, { "knee", 3.0f }, { "makeup", 0.0f },
                        { "dryWet", 100.0f }, { "bypass", 0.0f } } },
                    { "150 ms. It recovers in the gaps but not inside them. This is "
                      "the ordinary working answer for a groove at this tempo.",
                      { { "release", 150.0f } } },
                    { "700 ms. It never gets back up before the next hit arrives, so "
                      "the gain reduction just sits there. The compressor has stopped "
                      "responding to the music and become a volume knob.",
                      { { "release", 700.0f } } }
                },
                {
                    "release", Bed::drumLoop,
                    40.0f, 800.0f, true, 5.0f,
                    Unit::proportion, 0.8f, 0.28f,
                    {}, " ms", 1.0f
                }
            },
            {
                "comp.knee",
                "Knee",
                "How abruptly compression arrives as the signal crosses the threshold.",
                "Widen the knee until the compressor stops announcing itself. You are "
                "listening for the moment gain reduction starts to become hard to "
                "point at.",
                {
                    { "Hard knee, 0 dB. Below the threshold nothing, above it the "
                      "full ratio. The transition has an edge you can hear.",
                      { { "threshold", -18.0f }, { "ratio", 8.0f }, { "attack", 8.0f },
                        { "release", 150.0f }, { "knee", 0.0f }, { "makeup", 0.0f },
                        { "dryWet", 100.0f }, { "bypass", 0.0f } } },
                    { "12 dB. The ratio now eases in over a 12 dB span around the "
                      "threshold, so quiet material gets a little compression and "
                      "loud material gets all of it. Same threshold, softer arrival.",
                      { { "knee", 12.0f } } }
                },
                {
                    "knee", Bed::drumLoop,
                    0.0f, 18.0f, false, 1.0f,
                    Unit::rangeFraction, 0.3f, 0.12f,
                    {}, " dB", 1.0f
                }
            },
            {
                "comp.makeup",
                "Makeup gain",
                "Puts back the level the compression took away.",
                "Match the compressed sound to the bypassed one. Louder always sounds "
                "better, and matching level is the only way to hear what the "
                "compressor actually did.",
                {
                    { "6 dB of gain reduction and no makeup. The compressed version "
                      "is quieter, so of course it sounds worse - you have not heard "
                      "compression yet, you have heard a level drop.",
                      { { "threshold", -20.0f }, { "ratio", 4.0f }, { "attack", 10.0f },
                        { "release", 150.0f }, { "knee", 6.0f }, { "makeup", 0.0f },
                        { "dryWet", 100.0f }, { "bypass", 0.0f } } },
                    { "Makeup up by about what the meter is taking off. Now the two "
                      "are the same loudness, and the difference you hear is the "
                      "actual difference.",
                      { { "makeup", 6.0f } } }
                },
                {
                    "makeup", Bed::bassNote,
                    0.0f, 12.0f, false, 0.5f,
                    Unit::decibels, 4.0f, 1.2f,
                    {}, " dB", 1.0f
                }
            },
            {
                "comp.dryWet",
                "Parallel compression",
                "Blends the compressed signal back in with the untouched one.",
                "Find the blend where the quiet detail comes up but the hits still "
                "have their shape. This is how you get density without flattening.",
                {
                    { "100% wet with heavy compression. Everything is level and "
                      "nothing has any dynamics left.",
                      { { "threshold", -28.0f }, { "ratio", 10.0f }, { "attack", 3.0f },
                        { "release", 120.0f }, { "knee", 3.0f }, { "makeup", 4.0f },
                        { "dryWet", 100.0f }, { "bypass", 0.0f } } },
                    { "40% wet. The squashed version lifts the quiet parts while the "
                      "dry version keeps the transients intact. You get the loudness "
                      "of the first without losing the shape of the second.",
                      { { "dryWet", 40.0f } } }
                },
                {
                    "dryWet", Bed::drumLoop,
                    15.0f, 85.0f, false, 5.0f,
                    Unit::rangeFraction, 0.3f, 0.1f,
                    {}, "%", 1.0f
                }
            }
        };
    }
}

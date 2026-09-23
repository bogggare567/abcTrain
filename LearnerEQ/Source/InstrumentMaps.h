#pragma once

#include <juce_graphics/juce_graphics.h>
#include "shared/dsp/EQCoefficients.h"
#include <array>
#include <functional>
#include <vector>

// Where the parts of an instrument live, and one real task for each.
//
// FrequencyZones is the general map - eight named ranges any sound can be
// talked about in. A mixer working on a kick does not think in those; they
// think "the body, the box, the click". This is that second map, one per
// instrument, with a short lesson attached: the steps of a job people do
// on that instrument every day, each ticked off when the bands on the
// curve actually do it (docs/design/approved-2026-09/LearnerEQ.png).
//
// The ranges are the conventional teaching ones - where the part *usually*
// is. The room, the mic and the player move them, and the panel says so.
// Nothing here is checked by ear (ADR 041): a step is done when the EQ is
// shaped that way, and whether it sounds better is the player's call.
namespace InstrumentMaps
{
    enum class ZoneKind { part, problem, edge };

    struct Zone
    {
        float lowHz, highHz;
        const char* key;        // i18n: eq.zone.<key>
        const char* english;
        ZoneKind kind;
    };

    // What a step asks of the curve.
    enum class Move { highPass, boost, cut };

    struct Step
    {
        Move move;
        float lowHz, highHz;
        float amountDb;          // for boost/cut: at least this much
        const char* key;         // i18n: eq.lesson.<instrument>.<n>
        const char* english;
    };

    struct Instrument
    {
        const char* key;         // i18n: eq.inst.<key>
        const char* english;
        std::vector<Zone> zones;
        const char* lessonTitle;
        std::vector<Step> steps;
    };

    inline const std::vector<Instrument>& all()
    {
        static const std::vector<Instrument> instruments {
            { "kick", "Kick",
              { { 50.0f, 100.0f, "body", "Body", ZoneKind::part },
                { 200.0f, 500.0f, "boxy", "Boxy", ZoneKind::problem },
                { 2000.0f, 5000.0f, "click", "Click", ZoneKind::edge } },
              "Make the kick sit under the bass",
              { { Move::highPass, 20.0f, 45.0f, 0.0f, "1", "Clean the rumble below the kick: high-pass at about 30 Hz." },
                { Move::boost, 50.0f, 100.0f, 2.0f, "2", "Find the body - 50-80 Hz - and give it about +3 dB." },
                { Move::cut, 200.0f, 500.0f, 2.0f, "3", "Sweep a narrow cut through 200-450 Hz until the cardboard goes away." },
                { Move::boost, 2000.0f, 5000.0f, 2.0f, "4", "Bring out the click at 2-5 kHz so it cuts through the bass." } } },

            { "snare", "Snare",
              { { 150.0f, 250.0f, "body", "Body", ZoneKind::part },
                { 400.0f, 900.0f, "ring", "Ring", ZoneKind::problem },
                { 2000.0f, 5000.0f, "crack", "Crack", ZoneKind::edge } },
              "A snare that cracks instead of ringing",
              { { Move::highPass, 60.0f, 120.0f, 0.0f, "1", "High-pass around 80 Hz: the kick lives below, not the snare." },
                { Move::boost, 150.0f, 250.0f, 2.0f, "2", "Give the body a little at 150-250 Hz." },
                { Move::cut, 400.0f, 900.0f, 3.0f, "3", "Find the ring between 400 and 900 Hz with a narrow boost, then turn it into a cut." },
                { Move::boost, 2000.0f, 5000.0f, 2.0f, "4", "Lift the crack at 2-5 kHz." } } },

            { "bass", "Bass",
              { { 40.0f, 100.0f, "weight", "Weight", ZoneKind::part },
                { 200.0f, 400.0f, "mud", "Mud", ZoneKind::problem },
                { 700.0f, 2000.0f, "definition", "Definition", ZoneKind::edge } },
              "A bass you can hear on small speakers",
              { { Move::highPass, 25.0f, 45.0f, 0.0f, "1", "High-pass at 30-40 Hz: below that is energy nobody hears." },
                { Move::cut, 200.0f, 400.0f, 2.0f, "2", "Cut the mud at 200-400 Hz, where bass and guitars pile up." },
                { Move::boost, 700.0f, 2000.0f, 2.0f, "3", "Boost the definition at 700 Hz-2 kHz: it is how a phone speaker hears a bass." } } },

            { "vocal", "Vocal",
              { { 150.0f, 350.0f, "mud", "Mud", ZoneKind::problem },
                { 800.0f, 1500.0f, "nasal", "Nasal", ZoneKind::problem },
                { 2500.0f, 5000.0f, "presence", "Presence", ZoneKind::edge },
                { 5000.0f, 9000.0f, "esses", "Esses", ZoneKind::problem } },
              "A voice in front of the mix",
              { { Move::highPass, 70.0f, 120.0f, 0.0f, "1", "High-pass at 80-100 Hz: handling noise and breath rumble, not voice." },
                { Move::cut, 150.0f, 350.0f, 2.0f, "2", "A wide cut in the mud at 150-350 Hz." },
                { Move::boost, 2500.0f, 5000.0f, 2.0f, "3", "A little presence at 2.5-5 kHz brings the words forward." },
                { Move::cut, 5000.0f, 9000.0f, 2.0f, "4", "If the esses get sharp, a small narrow cut at 5-9 kHz." } } },

            { "acoustic", "Acoustic guitar",
              { { 80.0f, 200.0f, "boom", "Boom", ZoneKind::problem },
                { 300.0f, 600.0f, "box", "Box", ZoneKind::problem },
                { 5000.0f, 12000.0f, "sparkle", "Sparkle", ZoneKind::edge } },
              "An acoustic that does not boom",
              { { Move::highPass, 70.0f, 130.0f, 0.0f, "1", "High-pass at about 100 Hz: the body resonance booms close to the mic." },
                { Move::cut, 300.0f, 600.0f, 2.0f, "2", "Cut the box at 300-600 Hz." },
                { Move::boost, 5000.0f, 12000.0f, 2.0f, "3", "A high shelf or bell at 5-12 kHz for the strings' sparkle." } } },

            { "piano", "Piano",
              { { 60.0f, 200.0f, "weight", "Weight", ZoneKind::part },
                { 250.0f, 500.0f, "mud", "Mud", ZoneKind::problem },
                { 2000.0f, 5000.0f, "clarity", "Clarity", ZoneKind::edge } },
              "A piano that stays clear in a band",
              { { Move::highPass, 30.0f, 70.0f, 0.0f, "1", "High-pass at 40-60 Hz to leave the bottom to the bass." },
                { Move::cut, 250.0f, 500.0f, 2.0f, "2", "Cut some mud at 250-500 Hz." },
                { Move::boost, 2000.0f, 5000.0f, 2.0f, "3", "Lift the clarity at 2-5 kHz so the chords read." } } },
        };

        return instruments;
    }

    // A band, as a step sees it.
    struct BandState
    {
        EQCoefficients::BandType type;
        float freqHz, gainDb;
    };

    inline bool isDone (const Step& step, const std::vector<BandState>& bands)
    {
        for (const auto& b : bands)
        {
            if (b.freqHz < step.lowHz || b.freqHz > step.highHz)
                continue;

            switch (step.move)
            {
                case Move::highPass:
                    if (b.type == EQCoefficients::BandType::highPass) return true;
                    break;

                case Move::boost:
                    if (EQCoefficients::usesGain (b.type) && b.gainDb >= step.amountDb) return true;
                    break;

                case Move::cut:
                    if (b.type == EQCoefficients::BandType::notch
                        || (EQCoefficients::usesGain (b.type) && b.gainDb <= -step.amountDb))
                        return true;
                    break;
            }
        }

        return false;
    }
}

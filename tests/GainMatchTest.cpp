#include <juce_core/juce_core.h>
#include <cmath>

#include "../shared/GainMatch.h"
#include "../Source/Games/EQGame.h"
#include "../Source/Games/FrequencyRangeGame.h"
#include "../Source/Games/ReverbGame.h"
#include "../Source/Games/DelayGame.h"
#include "../Source/Games/StereoWidthGame.h"
#include "../Source/Games/DistortionGame.h"
#include "../Source/Games/CompressionGame.h"
#include "../Source/Games/PanGame.h"

// One test for a rule that has to hold across eight exercises at once:
// **switching A/B must not change the loudness.**
//
// Every one of them hides a single change and asks you to hear it. If the
// processed side is louder there is a second change, and loudness is the
// easiest difference there is - so the round quietly stops being about
// reverb or width or a frequency and becomes "which one is louder". A
// player can then answer correctly for years while hearing the wrong
// thing, which is worse than not learning at all.
//
// Written as one shared check rather than eight separate ones on purpose:
// this is a property of the whole product, and a per-game version would
// let a ninth exercise arrive without anybody noticing it was exempt.
//
// DBGame is deliberately absent. Its A/B *is* the loudness question.
class GainMatchTest : public juce::UnitTest
{
public:
    GainMatchTest() : juce::UnitTest ("Gain match", "Games") {}

    void runTest() override
    {
        beginTest ("the helper restores what processing took, and refuses the impossible");
        {
            expectWithinAbsoluteError (GainMatch::from (0.4f, 0.2f), 2.0f, 0.001f);
            expectWithinAbsoluteError (GainMatch::from (0.2f, 0.4f), 0.5f, 0.001f);
            expectWithinAbsoluteError (GainMatch::from (0.3f, 0.3f), 1.0f, 0.001f);

            // Silence on either side would ask for an infinite or zero
            // gain; a NaN reaching the audio thread is a far worse bug
            // than a level left unmatched.
            expectWithinAbsoluteError (GainMatch::from (0.5f, 0.0f), 1.0f, 0.001f);
            expectWithinAbsoluteError (GainMatch::from (0.0f, 0.5f), 1.0f, 0.001f);
        }

        beginTest ("A/B is level-matched in every exercise that has one");
        {
            EQGame eq;
            FrequencyRangeGame range;
            ReverbGame reverb;
            DelayGame delay;
            StereoWidthGame width;
            DistortionGame distortion;
            CompressionGame compression;
            PanGame pan;

            struct Entry { const char* name; Game* game; };

            const Entry games[] = {
                { "EQGame",           &eq },
                { "FrequencyRange",   &range },
                { "ReverbGame",       &reverb },
                { "DelayGame",        &delay },
                { "StereoWidthGame",  &width },
                { "DistortionGame",   &distortion },
                { "CompressionGame",  &compression },
                { "PanGame",          &pan },
            };

            for (const auto& entry : games)
            {
                if (! entry.game->supportsBeforeAfter())
                    continue;

                entry.game->prepare ({ 44100.0, 512, 2 });

                for (const auto level : { 1, 5, 10 })
                {
                    entry.game->setDifficulty (level);

                    // Averaged over rounds rather than judged on one. Every
                    // exercise redraws noise and, for the burst games, a
                    // phase - scatter that is identical either side of the
                    // A/B switch and says nothing about it. What must not
                    // exist is a systematic difference.
                    auto dryTotal = 0.0, wetTotal = 0.0;
                    constexpr int rounds = 12;

                    for (int round = 0; round < rounds; ++round)
                    {
                        entry.game->newRound();

                        dryTotal += renderRms (*entry.game, false);
                        wetTotal += renderRms (*entry.game, true);
                    }

                    const auto dry = dryTotal / rounds;
                    const auto wet = wetTotal / rounds;

                    expect (dry > 0.0 && wet > 0.0,
                             juce::String (entry.name) + " rendered silence");

                    const auto differenceDb =
                        std::abs (juce::Decibels::gainToDecibels ((float) (wet / dry)));

                    // 1.5 dB. Below roughly 1 dB a level difference stops
                    // being reliably audible at all - the same threshold
                    // DBGame's tolerance stops at, and for the same reason
                    // - so this is comfortably inside "cannot be answered
                    // with".
                    expect (differenceDb < 1.5f,
                             juce::String (entry.name) + " at level " + juce::String (level)
                                 + ": processed is " + juce::String (differenceDb, 2)
                                 + " dB away from untreated - answerable by volume");
                }
            }
        }
    }

private:
    // Five seconds. The burst exercises repeat about once a second, so a
    // shorter window lands sometimes on a hit and sometimes on a decay
    // tail, which is several dB of noise on a measurement that is looking
    // for a fraction of one.
    static double renderRms (Game& game, bool processed)
    {
        game.setPlayProcessed (processed);

        juce::AudioBuffer<float> buffer (2, 44100 * 5);
        buffer.clear();
        game.process (buffer);

        return (double) GainMatch::rms (buffer);
    }
};

static GainMatchTest gainMatchTest;

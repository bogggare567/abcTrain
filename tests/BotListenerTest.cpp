#include <juce_core/juce_core.h>
#include "../Source/BotListener.h"

// The bots are psychometric functions, not dice (ADR 046). What has to hold
// for a battle with one to feel fair and readable:
//  - harder rounds are never easier for a bot;
//  - on an easy round it is nearly sure, but not quite (lapse);
//  - on an impossible round it guesses, never worse than chance;
//  - each specialist really is the strongest at its own thing;
//  - with a seeded Random the draws follow the curve.
class BotListenerTest : public juce::UnitTest
{
public:
    BotListenerTest() : juce::UnitTest ("BotListener", "Live") {}

    void runTest() override
    {
        using namespace BotListener;

        beginTest ("the chance of a right answer falls as the level rises, and stays between guess and 1 - lapse");
        for (int b = 0; b < numBots; ++b)
            for (int game = 0; game < numGames; ++game)
                for (int choices : { 2, 3, 5 })
                for (int bucket = -1; bucket < 7; ++bucket)
                {
                    const auto bot = (Bot) b;
                    const auto guess = 1.0f / (float) choices;
                    auto previous = 2.0f;

                    for (int level = 1; level <= 10; ++level)
                    {
                        const auto p = chanceOfRight (bot, game, level, choices, bucket);
                        expect (p <= previous + 1.0e-6f, juce::String (idOf (bot)) + ": level " + juce::String (level) + " easier than the one before");
                        expect (p >= guess - 1.0e-6f, "never worse than guessing");
                        expect (p <= 1.0f - profileOf (bot).lapse + 1.0e-6f, "never better than its attention allows");
                        previous = p;
                    }

                    expect (chanceOfRight (bot, game, 1, choices, bucket) > 0.75f, "level 1 is nearly sure, even in a bot's weakest part");
                }

        beginTest ("the literature shows through: who is best where (docs/research/2026-09-bot-hearing.md)");
        {
            const auto best = [] (int game, int bucket)
            {
                auto bestBot = Bot::hound;
                auto bestT = -1.0f;
                for (int b = 0; b < numBots; ++b)
                {
                    const auto t = thresholdOf ((Bot) b, game, bucket);
                    if (t > bestT) { bestT = t; bestBot = (Bot) b; }
                }
                return bestBot;
            };

            expect (best (0, 0) == Bot::viper, "sub-bass: the python (best 80-160 Hz, by vibration)");
            expect (best (0, 1) == Bot::viper, "bass: the python");
            expect (best (8, 0) == Bot::viper, "range, sub-bass: the python");
            expect (best (3, 2) == Bot::elephant, "pan, centre: the elephant (MAA ~1 deg)");
            expect (best (6, 3) == Bot::elephant, "width: the elephant");
            expect (best (4, 0) == Bot::bat, "slapback delay: the bat");
            expect (best (1, 0) == Bot::bat, "weak compression (transients): the bat");

            // Within one bot, the parts of an exercise differ the way its ear does.
            expect (thresholdOf (Bot::cat, 0, 6) > thresholdOf (Bot::cat, 0, 0) + 3.0f, "the cat: air far above sub-bass");
            expect (thresholdOf (Bot::viper, 0, 0) > thresholdOf (Bot::viper, 0, 6) + 2.5f, "the python: the reverse");
            expect (thresholdOf (Bot::owl, 0, 5) > thresholdOf (Bot::owl, 0, 1) + 3.0f, "the owl: presence over bass");
        }

        beginTest ("an unknown part of the exercise falls back to the exercise's mean");
        {
            expectWithinAbsoluteError (thresholdOf (Bot::owl, 3, -1), profileOf (Bot::owl).threshold[3], 1.0e-5f);
            expectWithinAbsoluteError (thresholdOf (Bot::owl, 3, 99), profileOf (Bot::owl).threshold[3], 1.0e-5f);
            expectWithinAbsoluteError (chanceOfRight (Bot::owl, 3, 5, 2, -1),
                                       chanceOfRight (Bot::owl, 3, 5, 2, 99), 1.0e-6f);
        }

        beginTest ("out-of-range input is clamped, not a crash");
        {
            expect (chanceOfRight ((Bot) 99, -3, 50) >= 0.5f - 1.0e-6f);
            expectWithinAbsoluteError (chanceOfRight (Bot::cat, 0, -5), chanceOfRight (Bot::cat, 0, 1), 1.0e-6f);
            expect (juce::String (idOf ((Bot) -1)).isNotEmpty());
        }

        beginTest ("draws follow the curve (seeded)");
        {
            juce::Random random (46);
            const int n = 4000;
            int right = 0;
            for (int i = 0; i < n; ++i)
                right += answers (Bot::owl, 3, 8, random) ? 1 : 0;

            const auto expected = chanceOfRight (Bot::owl, 3, 8);
            expectWithinAbsoluteError ((float) right / (float) n, expected, 0.03f);
        }

        beginTest ("thinking time: slower on harder rounds, and the elephant slower than the bat");
        {
            juce::Random random (1);
            int bat = 0, elephant = 0, easy = 0, hard = 0;
            for (int i = 0; i < 200; ++i)
            {
                bat += thinkingMs (Bot::bat, 5, random);
                elephant += thinkingMs (Bot::elephant, 5, random);
                easy += thinkingMs (Bot::hound, 1, random);
                hard += thinkingMs (Bot::hound, 10, random);
            }
            expect (elephant > bat);
            expect (hard > easy);
        }
    }
};

static BotListenerTest botListenerTest;

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
                {
                    const auto bot = (Bot) b;
                    const auto guess = 1.0f / (float) choices;
                    auto previous = 2.0f;

                    for (int level = 1; level <= 10; ++level)
                    {
                        const auto p = chanceOfRight (bot, game, level, choices);
                        expect (p <= previous + 1.0e-6f, juce::String (idOf (bot)) + ": level " + juce::String (level) + " easier than the one before");
                        expect (p >= guess - 1.0e-6f, "never worse than guessing");
                        expect (p <= 1.0f - profileOf (bot).lapse + 1.0e-6f, "never better than its attention allows");
                        previous = p;
                    }

                    expect (chanceOfRight (bot, game, 1, choices) > 0.8f, "level 1 is nearly sure for anyone");
                }

        beginTest ("each specialist is the strongest bot on its own exercises");
        {
            const auto best = [] (int game)
            {
                auto bestBot = Bot::hound;
                auto bestP = -1.0f;
                for (int b = 0; b < numBots; ++b)
                {
                    const auto p = chanceOfRight ((Bot) b, game, 7);
                    if (p > bestP) { bestP = p; bestBot = (Bot) b; }
                }
                return bestBot;
            };

            expect (best (0) == Bot::cat, "band: the cat");
            expect (best (3) == Bot::owl, "pan: the owl");
            expect (best (6) == Bot::owl, "width: the owl");
            expect (best (4) == Bot::bat, "delay: the bat");
            expect (best (5) == Bot::viper, "distortion: the viper");
            expect (best (8) == Bot::viper, "range: the viper");
            expect (best (7) == Bot::elephant, "gain: the elephant");
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

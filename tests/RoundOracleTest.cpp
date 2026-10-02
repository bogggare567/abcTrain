#include <juce_core/juce_core.h>
#include "../Source/GameManager.h"

// The battle server's judge (tools/RoundOracle, ADR 056) answers from a
// game it keeps running, and every app builds the round in a game that has
// played other rounds before. Both are right only if a seeded round depends
// on (exercise, level, seed) alone. This holds that: a fresh engine and one
// with a history of other rounds - seeded and not, other levels - must name
// the same right answer.
class RoundOracleTest : public juce::UnitTest
{
public:
    RoundOracleTest() : juce::UnitTest ("RoundOracle", "Live") {}

    static juce::String keyOf (Game& game)
    {
        return game.usesContinuousScale()
                   ? "c" + juce::String (game.getCorrectNormalised(), 4) + "/" + juce::String (game.getToleranceNormalised(), 4)
                   : "d" + juce::String (game.getCorrectChoiceIndex());
    }

    static juce::String ask (GameManager& m, int exercise, int level, juce::int64 seed)
    {
        auto& game = m.getGame (exercise);
        game.setSeededRounds (true);
        game.setDifficulty (level);
        game.seedNextRound (seed);
        game.newRound();
        return keyOf (game);
    }

    void runTest() override
    {
        beginTest ("a seeded round's answer does not depend on what the engine played before");

        GameManager used;
        used.prepare ({ 44100.0, 512, 2 });
        juce::Random r (7);

        for (int exercise = 0; exercise < used.getNumGames(); ++exercise)
            for (int k = 0; k < 6; ++k)
            {
                // Some history: unseeded rounds at a random level.
                auto& g = used.getGame (exercise);
                g.setSeededRounds (false);
                g.setDifficulty (1 + r.nextInt (10));
                for (int n = 0; n < 3; ++n)
                    g.newRound();

                const auto level = 1 + r.nextInt (10);
                const auto seed = (juce::int64) r.nextInt (1 << 30);

                GameManager fresh;
                fresh.prepare ({ 48000.0, 512, 2 });
                expectEquals (ask (used, exercise, level, seed), ask (fresh, exercise, level, seed),
                              "exercise " + juce::String (exercise) + " level " + juce::String (level));
            }
    }
};

static RoundOracleTest roundOracleTest;

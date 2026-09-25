#include <juce_core/juce_core.h>
#include "../Source/GameManager.h"

// Online battles (ADR 048): the server sends a seed, both apps build the
// round from it. This holds that one seed is one round on two computers -
// even when the two players have trained differently before (another round
// history, other weak spots weighting the draw), which is always the case.
class SeededRoundTest : public juce::UnitTest
{
public:
    SeededRoundTest() : juce::UnitTest ("SeededRound", "Games") {}

    // What a round asks, as text: two rounds are "the same" when every
    // answer the player could give is judged the same on both.
    static juce::String fingerprint (Game& g)
    {
        juce::String s;
        s << g.getNumChoices() << "|";
        for (int i = 0; i < g.getNumChoices(); ++i)
            s << g.getChoiceKey (i) << ",";
        s << "|" << g.getCorrectChoiceIndex();
        if (g.usesContinuousScale())
            s << "|" << juce::String (g.getCorrectNormalised(), 5) << "|" << juce::String (g.getToleranceNormalised(), 5);
        return s;
    }

    void runTest() override
    {
        for (int index = 0; index < 9; ++index)
        {
            GameManager left, right;
            auto& a = left.getGame (index);
            auto& b = right.getGame (index);
            beginTest (a.getName() + ": one seed, one round - whatever came before");

            // Different pasts: `a` has played a dozen rounds and has weak
            // spots; `b` is fresh.
            a.setDifficulty (3);
            for (int i = 0; i < 12; ++i)
            {
                a.newRound();
                a.submitAnswer (0);
            }
            a.setBucketWeights ({ 5.0f, 0.2f, 3.0f, 0.1f, 4.0f, 0.3f, 2.0f, 0.5f, 1.0f, 6.0f });

            int same = 0;
            for (juce::int64 seed : { 11, 20260925, 777777, 123456789, 42, 9001 })
            {
                for (int level : { 1, 5, 10 })
                {
                    for (auto* g : { &a, &b })
                    {
                        g->setSeededRounds (true);
                        g->setDifficulty (level);
                        g->seedNextRound (seed);
                        g->newRound();
                    }

                    const auto fa = fingerprint (a), fb = fingerprint (b);
                    expectEquals (fa, fb, "seed " + juce::String (seed) + " level " + juce::String (level));
                    same += fa == fb ? 1 : 0;
                }
            }

            expectEquals (same, 18);
        }
    }
};

static SeededRoundTest seededRoundTest;

#include <juce_core/juce_core.h>
#include <map>
#include <functional>
#include <cmath>
#include "../Source/Games/FrequencyRangeGame.h"

class FrequencyRangeGameTest : public juce::UnitTest
{
public:
    FrequencyRangeGameTest() : juce::UnitTest ("FrequencyRangeGame", "Games") {}

    void runTest() override
    {
        beginTest ("two alternatives, and harder levels offer neighbouring ranges");
        {
            FrequencyRangeGame game;

            // Spectral order. Neighbours share a boundary and are a real
            // question; sub-bass against air is not.
            const juce::StringArray order {
                "Sub-bass", "Bass", "Low-mids", "Mids", "High-mids", "Presence", "Air"
            };

            checkTwoAlternative (game, order,
                [&order] (const juce::String& a, const juce::String& b)
                { return std::abs ((float) order.indexOf (a) - (float) order.indexOf (b)) / 6.0f; });
        }


        beginTest ("the boosted frequency stays inside its own named range");
        {
            // A frequency that wandered past the boundary would mark a
            // correct answer wrong, which is the worst thing an exercise
            // can do. Checked at every level, since how much of the range
            // is in play is now the level's business.
            FrequencyRangeGame game;
            game.prepare ({ 44100.0, 512, 2 });

            for (int level = 1; level <= 10; ++level)
            {
                game.setDifficulty (level);

                for (int round = 0; round < 200; ++round)
                {
                    game.newRound();

                    const auto& range = FrequencyRangeGame::ranges[(size_t) game.getCorrectRangeForTest()];
                    const auto hz = game.getCorrectFrequencyHzForTest();

                    expect (hz >= range.lowHz && hz <= range.highHz,
                             juce::String (hz, 1) + " Hz is outside " + range.label
                                 + " at level " + juce::String (level));
                }
            }
        }

        beginTest ("easy levels stay near the middle of a range, hard ones reach its edges");
        {
            // This is the family idea applied to a continuum: the middle
            // of Bass is the archetypal Bass, the boundary with Low-mids
            // is the hard case. Measured as how close to the boundary the
            // draw gets, in octaves, so the two ends of the range count
            // the same way.
            FrequencyRangeGame game;
            game.prepare ({ 44100.0, 512, 2 });

            const auto closestApproachToBoundary = [&] (int level)
            {
                game.setDifficulty (level);
                auto closest = 10.0f;

                for (int round = 0; round < 400; ++round)
                {
                    game.newRound();

                    const auto& range = FrequencyRangeGame::ranges[(size_t) game.getCorrectRangeForTest()];
                    const auto hz = game.getCorrectFrequencyHzForTest();

                    const auto toLow = std::log2 (hz / range.lowHz);
                    const auto toHigh = std::log2 (range.highHz / hz);
                    closest = juce::jmin (closest, juce::jmin (toLow, toHigh));
                }

                return closest;
            };

            const auto easy = closestApproachToBoundary (1);
            const auto hard = closestApproachToBoundary (10);

            expect (hard < easy,
                     "level 10 got no closer to a range boundary than level 1 ("
                         + juce::String (hard, 3) + " vs " + juce::String (easy, 3) + " octaves)");
            expect (hard < 0.05f, "level 10 never reached a range boundary");
        }

        beginTest ("the bump narrows as the level rises");
        {
            // A broad lift is what a named range sounds like; a narrow one
            // is a single tone that happens to live in it. Both are real
            // questions, and which one you get should follow the level
            // rather than the draw.
            FrequencyRangeGame game;
            game.prepare ({ 44100.0, 512, 2 });

            game.setDifficulty (1);
            game.newRound();
            const auto broad = game.getFilterQForTest();

            game.setDifficulty (10);
            game.newRound();
            const auto narrow = game.getFilterQForTest();

            expect (narrow > broad,
                     "Q did not rise with level (" + juce::String (broad, 2)
                         + " -> " + juce::String (narrow, 2) + ")");
        }
    }

private:
    // One shared shape for the three games that became two-alternative:
    // the pair must always be two different real names, and harder levels
    // must offer closer pairs. The second is measured over many rounds
    // rather than asserted, because it is a claim about a distribution.
    template <typename GameType>
    void checkTwoAlternative (GameType& game, const juce::StringArray& knownLabels,
                               std::function<float (const juce::String&, const juce::String&)> distance)
    {
        for (int level = 1; level <= 10; ++level)
        {
            game.setDifficulty (level);
            expectEquals (game.getNumChoices(), 2, "level " + juce::String (level));
        }

        const auto averageDistance = [&] (int level)
        {
            game.setDifficulty (level);

            auto total = 0.0f;
            constexpr int rounds = 400;

            for (int i = 0; i < rounds; ++i)
            {
                game.newRound();

                const auto a = game.getChoiceLabel (0);
                const auto b = game.getChoiceLabel (1);

                expect (knownLabels.contains (a), "unknown label: " + a);
                expect (knownLabels.contains (b), "unknown label: " + b);
                expect (a != b, "the same choice offered twice: " + a);

                total += distance (a, b);
            }

            return total / (float) rounds;
        };

        const auto easy = averageDistance (1);
        const auto hard = averageDistance (10);

        expect (hard < easy,
                "level 10 pairs should be closer than level 1 pairs: "
                    + juce::String (hard) + " vs " + juce::String (easy));
    }
};

static FrequencyRangeGameTest frequencyRangeGameTest;

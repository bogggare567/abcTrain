#include <juce_core/juce_core.h>
#include <map>
#include <functional>
#include <cmath>
#include "../Source/Games/StereoWidthGame.h"
#include <cmath>

class StereoWidthGameTest : public juce::UnitTest
{
public:
    StereoWidthGameTest() : juce::UnitTest ("StereoWidthGame", "Games") {}

    void runTest() override
    {
        beginTest ("two alternatives, and harder levels offer closer widths");
        {
            StereoWidthGame game;

            const std::map<juce::String, float> position {
                { "Narrow", 0.0f }, { "Normal", 0.33f },
                { "Wide", 0.66f }, { "Extra Wide", 1.0f },
            };

            juce::StringArray known;
            for (const auto& entry : position)
                known.add (entry.first);

            checkTwoAlternative (game, known,
                [&position] (const juce::String& a, const juce::String& b)
                { return std::abs (position.at (a) - position.at (b)); });
        }


        beginTest ("keeping the low end mono never collapses the stereo signal");
        {
            // The family varies *how* the width is reached - how much of
            // the bottom stays centred. Pushed too far that would leave
            // two identical channels, which is a stereo-width exercise
            // playing mono. Driven through every voicing directly rather
            // than hoping the random draw reaches them.
            for (const auto& variant : StereoWidthGame::family())
            {
                StereoWidthGame game;
                game.prepare ({ 44100.0, 512, 2 });
                game.setDifficulty (10);   // narrowest widths, worst case

                // Run enough rounds that every voicing gets used with the
                // narrow end of the width table.
                for (int round = 0; round < 40; ++round)
                {
                    game.newRound();

                    juce::AudioBuffer<float> buffer (2, 4096);
                    buffer.clear();
                    game.process (buffer);

                    auto difference = 0.0;
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                    {
                        const auto d = buffer.getSample (0, i) - buffer.getSample (1, i);
                        difference += (double) d * (double) d;
                    }

                    expect (std::sqrt (difference / (double) buffer.getNumSamples()) > 1.0e-4,
                             "left and right were identical - no side signal left");
                }

                juce::ignoreUnused (variant);
            }
        }

        beginTest ("the family runs from widened-whole to mostly-centred");
        {
            const auto& variants = StereoWidthGame::family();
            expect (variants.size() >= 3, "fewer than three ways of arriving at a width");

            auto highest = 0.0f, lowest = 1.0f;
            auto plainOne = false;

            for (const auto& variant : variants)
            {
                highest = juce::jmax (highest, variant.archetypal);
                lowest = juce::jmin (lowest, variant.archetypal);

                if (variant.monoBelowHz <= 0.0f)
                    plainOne = true;
            }

            expect (plainOne, "no voicing widens the side signal whole");
            expect (highest > 0.9f, "no clear example");
            expect (lowest < 0.35f, "no borderline example");
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

static StereoWidthGameTest stereoWidthGameTest;

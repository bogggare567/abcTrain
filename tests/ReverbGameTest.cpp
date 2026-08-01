#include <juce_core/juce_core.h>
#include "../Source/Games/ReverbGame.h"
#include <cmath>

class ReverbGameTest : public juce::UnitTest
{
public:
    ReverbGameTest() : juce::UnitTest ("ReverbGame", "Games") {}

    void runTest() override
    {
        beginTest ("always exactly two choices, at every level");
        {
            // The rule that replaced "difficulty adds buttons". More
            // options is more reading and more luck, not finer hearing;
            // two alternatives is what a listening test uses, and
            // difficulty became how close together the two are.
            ReverbGame game;

            for (int level = 1; level <= 10; ++level)
            {
                game.setDifficulty (level);
                expectEquals (game.getNumChoices(), 2, "level " + juce::String (level));
            }
        }

        beginTest ("the two labels are real, different type names");
        {
            ReverbGame game;

            // Many rounds, because the pair is drawn fresh each time and a
            // bug that mixes the pair up with the answer index would only
            // show on some draws.
            for (int level : { 1, 5, 10 })
            {
                game.setDifficulty (level);

                for (int round = 0; round < 60; ++round)
                {
                    game.newRound();

                    const auto a = game.getChoiceLabel (0);
                    const auto b = game.getChoiceLabel (1);

                    expect (a.isNotEmpty() && b.isNotEmpty(), "empty label");
                    expect (a != b, "the same type offered twice: " + a);
                }
            }
        }

        beginTest ("harder levels offer pairs that are closer together");
        {
            // The whole claim of the redesign, measured rather than
            // asserted: draw many rounds at each end and compare how far
            // apart the offered types are on the character axis.
            ReverbGame game;

            const auto averageDistance = [&game] (int level)
            {
                game.setDifficulty (level);

                auto total = 0.0f;
                constexpr int rounds = 400;

                for (int i = 0; i < rounds; ++i)
                {
                    game.newRound();
                    total += ReverbGame::confusabilityForTest (game.getChoiceLabel (0),
                                                                game.getChoiceLabel (1));
                }

                return total / (float) rounds;
            };

            const auto easy = averageDistance (1);
            const auto hard = averageDistance (10);

            expect (hard < easy,
                    "level 10 pairs should be closer than level 1 pairs: "
                        + juce::String (hard) + " vs " + juce::String (easy));
        }

        beginTest ("no level collapses to one question, and no two levels are the same");
        {
            // The four things the first version of the pair rule actually
            // did, all of which compiled and passed every test there was:
            // level 10 offered one pair forever, levels 3 to 7 were
            // identical to each other, Spring filled 80% of level 1, and
            // Spring never appeared above level 1 at all. Measured here so
            // none of them can come back quietly.
            ReverbGame game;

            const auto pairsSeenAt = [&game] (int level)
            {
                game.setDifficulty (level);

                juce::StringArray seen;
                for (int i = 0; i < 600; ++i)
                {
                    game.newRound();

                    juce::StringArray both { game.getChoiceLabel (0), game.getChoiceLabel (1) };
                    both.sort (true);
                    seen.addIfNotAlreadyThere (both.joinIntoString ("/"));
                }

                seen.sort (true);
                return seen;
            };

            juce::String previous;
            auto runLength = 1;

            for (int level = 1; level <= 10; ++level)
            {
                const auto seen = pairsSeenAt (level);
                const auto asText = seen.joinIntoString (",");

                expect (seen.size() >= 3,
                         "level " + juce::String (level) + " only ever offers "
                             + juce::String (seen.size()) + " pair(s)");

                // Ten levels sliding across six window positions means two
                // *adjacent* levels sometimes share a pool, and that is
                // fine - the family draw still differs between them. What
                // must not happen is a plateau: five levels in a row that
                // were indistinguishable is what the old rule produced.
                runLength = (asText == previous) ? runLength + 1 : 1;

                expect (runLength <= 2,
                         "levels " + juce::String (level - runLength + 1) + " to "
                             + juce::String (level) + " all offer exactly the same pairs");

                previous = asText;
            }

            expect (pairsSeenAt (1).joinIntoString (",") != pairsSeenAt (10).joinIntoString (","),
                     "level 10 asks the same questions as level 1");
        }

        beginTest ("Spring is available at both ends of the difficulty range");
        {
            // Spring is the one type whose character is a mechanism rather
            // than a size. It belongs at the easy end (against a hall,
            // unmistakable) *and* at the hard end (against a plate, two
            // pieces of metal) - and the blanket "always far from
            // everything" rule it used to have gave it only the first.
            ReverbGame game;

            const auto springAppearsAt = [&game] (int level)
            {
                game.setDifficulty (level);

                for (int i = 0; i < 600; ++i)
                {
                    game.newRound();

                    if (game.getChoiceLabel (0) == "Spring" || game.getChoiceLabel (1) == "Spring")
                        return true;
                }

                return false;
            };

            expect (springAppearsAt (1), "Spring never appears at level 1");
            expect (springAppearsAt (10), "Spring never appears at level 10");
        }

        beginTest ("produces a non-silent buffer after newRound()");
        {
            ReverbGame game;
            game.setDifficulty (10);
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec); // prepare() calls newRound() once

            juce::AudioBuffer<float> buffer (2, 512);
            game.process (buffer);

            float maxAbs = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    maxAbs = juce::jmax (maxAbs, std::abs (buffer.getSample (ch, i)));

            expect (maxAbs > 0.0f);
        }

        beginTest ("correct answer increases the score");
        {
            ReverbGame game;
            game.setDifficulty (10);
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);

            expect (game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 1);
        }

        beginTest ("wrong answer does not increase the score");
        {
            ReverbGame game;
            game.setDifficulty (10);
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            const auto wrong = (correct + 1) % game.getNumChoices();
            game.submitAnswer (wrong);

            expect (! game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 0);
        }

        beginTest ("a second answer in the same round is ignored");
        {
            ReverbGame game;
            game.setDifficulty (10);
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);
            game.submitAnswer ((correct + 1) % game.getNumChoices());

            expectEquals (game.getRoundsPlayed(), 1);
            expectEquals (game.getChosenChoiceIndex(), correct);
        }
    }
};

static ReverbGameTest reverbGameTest;

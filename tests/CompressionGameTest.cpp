#include <juce_core/juce_core.h>
#include "../Source/Games/CompressionGame.h"

class CompressionGameTest : public juce::UnitTest
{
public:
    CompressionGameTest() : juce::UnitTest ("CompressionGame", "Games") {}

    void runTest() override
    {
        beginTest ("always exactly two choices");
        {
            CompressionGame game;

            for (int level = 1; level <= 10; ++level)
            {
                game.setDifficulty (level);
                expectEquals (game.getNumChoices(), 2, "level " + juce::String (level));
            }
        }

        beginTest ("correct answer increases the score");
        {
            CompressionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);

            expect (game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 1);
        }

        beginTest ("wrong answer does not increase the score");
        {
            CompressionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            const auto wrong = (correct + 1) % CompressionGame::numLevels;
            game.submitAnswer (wrong);

            expect (! game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 0);
        }

        beginTest ("feedback text is only available after answering");
        {
            CompressionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            expect (game.getFeedbackText().isEmpty());

            game.submitAnswer (game.getCorrectChoiceIndex());
            expect (game.getFeedbackText().isNotEmpty());
        }

        beginTest ("a second answer in the same round is ignored");
        {
            CompressionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);
            game.submitAnswer ((correct + 1) % CompressionGame::numLevels);

            expectEquals (game.getRoundsPlayed(), 1);
            expectEquals (game.getChosenChoiceIndex(), correct);
        }

        beginTest ("the pair is always two different real strengths");
        {
            CompressionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            const juce::StringArray known { "Weak", "Medium", "Strong" };

            for (const int level : { 1, 5, 10 })
            {
                game.setDifficulty (level);
                game.prepare (spec);

                for (int round = 0; round < 40; ++round)
                {
                    game.newRound();

                    const auto a = game.getChoiceLabel (0);
                    const auto b = game.getChoiceLabel (1);

                    expect (known.contains (a), "unknown label " + a);
                    expect (known.contains (b), "unknown label " + b);
                    expect (a != b, "the same strength offered twice: " + a);

                    const auto correct = game.getCorrectChoiceIndex();
                    game.submitAnswer (correct);
                    expect (game.wasLastAnswerCorrect());
                }
            }
        }

        beginTest ("harder levels offer neighbouring strengths more often");
        {
            // Weak-against-Strong is the giveaway pair; either neighbour
            // pair is the real question. The claim is that climbing the
            // levels replaces the first with the second - measured, not
            // asserted.
            CompressionGame game;

            const auto neighbourShare = [&game] (int level)
            {
                game.setDifficulty (level);

                auto neighbours = 0;
                constexpr int rounds = 400;

                for (int i = 0; i < rounds; ++i)
                {
                    game.newRound();

                    juce::StringArray offered;
                    offered.add (game.getChoiceLabel (0));
                    offered.add (game.getChoiceLabel (1));

                    // The easy pair is the only one without "Medium" in it.
                    if (offered.contains ("Medium"))
                        ++neighbours;
                }

                return (float) neighbours / (float) rounds;
            };

            const auto easy = neighbourShare (1);
            const auto hard = neighbourShare (10);

            expect (hard > easy,
                    "level 10 should offer neighbours more often: "
                        + juce::String (hard) + " vs " + juce::String (easy));
            expect (hard > 0.95f, "level 10 should have dropped the giveaway pair entirely");
        }
    }
};

static CompressionGameTest compressionGameTest;

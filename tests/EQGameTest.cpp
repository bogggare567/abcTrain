#include <juce_core/juce_core.h>
#include "../Source/Games/EQGame.h"

class EQGameTest : public juce::UnitTest
{
public:
    EQGameTest() : juce::UnitTest ("EQGame", "Games") {}

    void runTest() override
    {
        beginTest ("exposes one choice per band");
        {
            EQGame game;
            expectEquals (game.getNumChoices(), EQGame::numBands);
        }

        beginTest ("prepare() starts an unanswered round");
        {
            EQGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            expect (! game.hasAnswered());
            expectEquals (game.getChosenChoiceIndex(), -1);
        }

        beginTest ("correct answer increases the score");
        {
            EQGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);

            expect (game.hasAnswered());
            expect (game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 1);
            expectEquals (game.getRoundsPlayed(), 1);
        }

        beginTest ("wrong answer does not increase the score");
        {
            EQGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            const auto wrong = (correct + 1) % EQGame::numBands;
            game.submitAnswer (wrong);

            expect (! game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 0);
            expectEquals (game.getRoundsPlayed(), 1);
        }

        beginTest ("a second answer in the same round is ignored");
        {
            EQGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);
            game.submitAnswer ((correct + 1) % EQGame::numBands);

            expectEquals (game.getRoundsPlayed(), 1);
            expectEquals (game.getChosenChoiceIndex(), correct);
        }

        beginTest ("setDifficulty doesn't change the band count and rounds still play");
        {
            EQGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };

            for (const int level : { 1, 5, 10 })
            {
                game.setDifficulty (level);
                game.prepare (spec); // re-prepares, calls newRound()

                expectEquals (game.getNumChoices(), EQGame::numBands);

                const auto correct = game.getCorrectChoiceIndex();
                game.submitAnswer (correct);
                expect (game.wasLastAnswerCorrect());
            }
        }
    }
};

static EQGameTest eqGameTest;

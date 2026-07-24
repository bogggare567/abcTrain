#include <juce_core/juce_core.h>
#include "../Source/Games/CompressionGame.h"

class CompressionGameTest : public juce::UnitTest
{
public:
    CompressionGameTest() : juce::UnitTest ("CompressionGame", "Games") {}

    void runTest() override
    {
        beginTest ("exposes 3 strength choices");
        {
            CompressionGame game;
            expectEquals (game.getNumChoices(), CompressionGame::numLevels);
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

        beginTest ("setDifficulty keeps the same 3 labels and choice count at every tier");
        {
            CompressionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };

            for (const int level : { 1, 5, 10 })
            {
                game.setDifficulty (level);
                game.prepare (spec);

                expectEquals (game.getNumChoices(), CompressionGame::numLevels);
                expectEquals (game.getChoiceLabel (0), juce::String ("Weak"));
                expectEquals (game.getChoiceLabel (1), juce::String ("Medium"));
                expectEquals (game.getChoiceLabel (2), juce::String ("Strong"));

                const auto correct = game.getCorrectChoiceIndex();
                game.submitAnswer (correct);
                expect (game.wasLastAnswerCorrect());
            }
        }
    }
};

static CompressionGameTest compressionGameTest;

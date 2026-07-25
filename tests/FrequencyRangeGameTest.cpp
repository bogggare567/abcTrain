#include <juce_core/juce_core.h>
#include "../Source/Games/FrequencyRangeGame.h"

class FrequencyRangeGameTest : public juce::UnitTest
{
public:
    FrequencyRangeGameTest() : juce::UnitTest ("FrequencyRangeGame", "Games") {}

    void runTest() override
    {
        beginTest ("exposes one choice per named range");
        {
            FrequencyRangeGame game;
            expectEquals (game.getNumChoices(), FrequencyRangeGame::numRanges);
        }

        beginTest ("prepare() starts an unanswered round");
        {
            FrequencyRangeGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            expect (! game.hasAnswered());
            expectEquals (game.getChosenChoiceIndex(), -1);
        }

        beginTest ("correct answer increases the score");
        {
            FrequencyRangeGame game;
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
            FrequencyRangeGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            const auto wrong = (correct + 1) % FrequencyRangeGame::numRanges;
            game.submitAnswer (wrong);

            expect (! game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 0);
            expectEquals (game.getRoundsPlayed(), 1);
        }

        beginTest ("a second answer in the same round is ignored");
        {
            FrequencyRangeGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);
            game.submitAnswer ((correct + 1) % FrequencyRangeGame::numRanges);

            expectEquals (game.getRoundsPlayed(), 1);
            expectEquals (game.getChosenChoiceIndex(), correct);
        }

        beginTest ("choice labels match the 7 standard range names");
        {
            FrequencyRangeGame game;
            const juce::StringArray expectedLabels {
                "Sub-bass", "Bass", "Low-mids", "Mids", "High-mids", "Presence", "Air"
            };
            expectEquals (game.getNumChoices(), expectedLabels.size());
            for (int i = 0; i < game.getNumChoices(); ++i)
                expectEquals (game.getChoiceLabel (i), expectedLabels[i]);
        }

        beginTest ("setDifficulty doesn't change the range count and rounds still play");
        {
            FrequencyRangeGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };

            for (const int level : { 1, 5, 10 })
            {
                game.setDifficulty (level);
                game.prepare (spec); // re-prepares, calls newRound()

                expectEquals (game.getNumChoices(), FrequencyRangeGame::numRanges);

                const auto correct = game.getCorrectChoiceIndex();
                game.submitAnswer (correct);
                expect (game.wasLastAnswerCorrect());
            }
        }

        beginTest ("process() produces a non-silent buffer");
        {
            FrequencyRangeGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            juce::AudioBuffer<float> buffer (2, 512);
            buffer.clear();
            game.process (buffer);

            expect (buffer.getMagnitude (0, 0, 512) > 0.0f);
        }
    }
};

static FrequencyRangeGameTest frequencyRangeGameTest;

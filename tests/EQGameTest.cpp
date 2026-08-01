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

        beginTest ("targets stay inside the audible band the ruler is wider than");
        {
            // The scale shows 20 Hz to 20 kHz because that is what a
            // spectrum is. The questions must not: a boost at 25 Hz is
            // inaudible on most speakers and one at 19 kHz to most adults,
            // and a round nobody can hear is a round answered by guessing.
            EQGame game;
            game.prepare ({ 44100.0, 512, 1 });

            for (int level = 1; level <= 10; ++level)
            {
                game.setDifficulty (level);

                auto lowest = 1.0e9f, highest = 0.0f;

                for (int round = 0; round < 400; ++round)
                {
                    game.newRound();

                    const auto hz = EQGame::normalisedToFrequency (game.getCorrectNormalised());
                    lowest = juce::jmin (lowest, hz);
                    highest = juce::jmax (highest, hz);
                }

                expect (lowest >= EQGame::targetLowHz * 0.999f,
                         "level " + juce::String (level) + " asked about "
                             + juce::String (lowest, 1) + " Hz");
                expect (highest <= EQGame::targetHighHz * 1.001f,
                         "level " + juce::String (level) + " asked about "
                             + juce::String (highest, 1) + " Hz");

                // ...and still uses the width it has, or the narrowing
                // would have quietly become a fixed pair of frequencies.
                expect (lowest < EQGame::targetLowHz * 1.5f, "never asks near the bottom");
                expect (highest > EQGame::targetHighHz * 0.7f, "never asks near the top");
            }
        }

        beginTest ("the ruler still spans everything you can hear");
        {
            // The complement of the check above: narrowing the draw must
            // not have narrowed the scale, or the exercise would be back to
            // teaching that music stops at 16 kHz.
            expectWithinAbsoluteError (EQGame::axisLowHz, 20.0f, 0.01f);
            expectWithinAbsoluteError (EQGame::axisHighHz, 20000.0f, 0.01f);

            EQGame game;
            const auto marks = game.getGridMarks();
            expect (! marks.empty(), "the ruler has no marks");

            for (const auto& mark : marks)
                expect (mark.normalised >= 0.0f && mark.normalised <= 1.0f,
                         "a grid mark sits off the scale");
        }
    }
};

static EQGameTest eqGameTest;

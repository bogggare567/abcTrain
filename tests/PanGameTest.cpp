#include <juce_core/juce_core.h>
#include "../Source/Games/PanGame.h"
#include <cmath>

class PanGameTest : public juce::UnitTest
{
public:
    PanGameTest() : juce::UnitTest ("PanGame", "Games") {}

    void runTest() override
    {
        beginTest ("exposes 5 pan position choices");
        {
            PanGame game;
            expectEquals (game.getNumChoices(), PanGame::numPositions);
        }

        beginTest ("produces a non-silent buffer after newRound()");
        {
            PanGame game;
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
            PanGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);

            expect (game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 1);
        }

        beginTest ("wrong answer does not increase the score");
        {
            PanGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            const auto wrong = (correct + 1) % PanGame::numPositions;
            game.submitAnswer (wrong);

            expect (! game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 0);
        }

        beginTest ("a second answer in the same round is ignored");
        {
            PanGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);
            game.submitAnswer ((correct + 1) % PanGame::numPositions);

            expectEquals (game.getRoundsPlayed(), 1);
            expectEquals (game.getChosenChoiceIndex(), correct);
        }

        beginTest ("setDifficulty keeps the same 5 labels and choice count at every tier");
        {
            PanGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };

            for (const int level : { 1, 5, 10 })
            {
                game.setDifficulty (level);
                game.prepare (spec);

                expectEquals (game.getNumChoices(), PanGame::numPositions);
                expectEquals (game.getChoiceLabel (0), juce::String ("Hard Left"));
                expectEquals (game.getChoiceLabel (2), juce::String ("Center"));
                expectEquals (game.getChoiceLabel (4), juce::String ("Hard Right"));

                const auto correct = game.getCorrectChoiceIndex();
                game.submitAnswer (correct);
                expect (game.wasLastAnswerCorrect());
            }
        }
    }
};

static PanGameTest panGameTest;

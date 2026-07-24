#include <juce_core/juce_core.h>
#include "../Source/Games/DelayGame.h"
#include <cmath>

class DelayGameTest : public juce::UnitTest
{
public:
    DelayGameTest() : juce::UnitTest ("DelayGame", "Games") {}

    void runTest() override
    {
        beginTest ("exposes 4 delay time choices");
        {
            DelayGame game;
            expectEquals (game.getNumChoices(), DelayGame::numDelayTimes);
        }

        beginTest ("produces a non-silent buffer after newRound()");
        {
            DelayGame game;
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
            DelayGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);

            expect (game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 1);
        }

        beginTest ("wrong answer does not increase the score");
        {
            DelayGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            const auto wrong = (correct + 1) % DelayGame::numDelayTimes;
            game.submitAnswer (wrong);

            expect (! game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 0);
        }

        beginTest ("a second answer in the same round is ignored");
        {
            DelayGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);
            game.submitAnswer ((correct + 1) % DelayGame::numDelayTimes);

            expectEquals (game.getRoundsPlayed(), 1);
            expectEquals (game.getChosenChoiceIndex(), correct);
        }

        beginTest ("setDifficulty keeps the same 4 fixed delay-time labels and choice count at every tier");
        {
            DelayGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };

            for (const int level : { 1, 5, 10 })
            {
                game.setDifficulty (level);
                game.prepare (spec);

                expectEquals (game.getNumChoices(), DelayGame::numDelayTimes);
                expectEquals (game.getChoiceLabel (0), juce::String ("50ms"));
                expectEquals (game.getChoiceLabel (1), juce::String ("150ms"));
                expectEquals (game.getChoiceLabel (2), juce::String ("300ms"));
                expectEquals (game.getChoiceLabel (3), juce::String ("500ms"));

                const auto correct = game.getCorrectChoiceIndex();
                game.submitAnswer (correct);
                expect (game.wasLastAnswerCorrect());
            }
        }
    }
};

static DelayGameTest delayGameTest;

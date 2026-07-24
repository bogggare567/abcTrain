#include <juce_core/juce_core.h>
#include "../Source/Games/StereoWidthGame.h"
#include <cmath>

class StereoWidthGameTest : public juce::UnitTest
{
public:
    StereoWidthGameTest() : juce::UnitTest ("StereoWidthGame", "Games") {}

    void runTest() override
    {
        beginTest ("exposes 4 stereo width choices");
        {
            StereoWidthGame game;
            expectEquals (game.getNumChoices(), StereoWidthGame::numWidths);
        }

        beginTest ("produces a non-silent, genuinely decorrelated stereo buffer");
        {
            StereoWidthGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec); // prepare() calls newRound() once

            juce::AudioBuffer<float> buffer (2, 512);
            game.process (buffer);

            float maxAbs = 0.0f;
            bool channelsDiffer = false;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto l = buffer.getSample (0, i);
                const auto r = buffer.getSample (1, i);
                maxAbs = juce::jmax (maxAbs, std::abs (l), std::abs (r));
                if (std::abs (l - r) > 1.0e-6f)
                    channelsDiffer = true;
            }

            expect (maxAbs > 0.0f);
            // The two independent PinkNoiseGenerator instances (see the
            // header comment on why this game needs two, unlike every
            // other game here) must actually decorrelate L from R -
            // otherwise the side signal would always be zero and width
            // would have nothing to affect.
            expect (channelsDiffer);
        }

        beginTest ("correct answer increases the score");
        {
            StereoWidthGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);

            expect (game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 1);
        }

        beginTest ("wrong answer does not increase the score");
        {
            StereoWidthGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            const auto wrong = (correct + 1) % StereoWidthGame::numWidths;
            game.submitAnswer (wrong);

            expect (! game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 0);
        }

        beginTest ("a second answer in the same round is ignored");
        {
            StereoWidthGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);
            game.submitAnswer ((correct + 1) % StereoWidthGame::numWidths);

            expectEquals (game.getRoundsPlayed(), 1);
            expectEquals (game.getChosenChoiceIndex(), correct);
        }

        beginTest ("setDifficulty keeps the same 4 labels and choice count at every tier");
        {
            StereoWidthGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };

            for (const int level : { 1, 5, 10 })
            {
                game.setDifficulty (level);
                game.prepare (spec);

                expectEquals (game.getNumChoices(), StereoWidthGame::numWidths);
                expectEquals (game.getChoiceLabel (0), juce::String ("Narrow"));
                expectEquals (game.getChoiceLabel (1), juce::String ("Normal"));
                expectEquals (game.getChoiceLabel (2), juce::String ("Wide"));
                expectEquals (game.getChoiceLabel (3), juce::String ("Extra Wide"));

                const auto correct = game.getCorrectChoiceIndex();
                game.submitAnswer (correct);
                expect (game.wasLastAnswerCorrect());
            }
        }
    }
};

static StereoWidthGameTest stereoWidthGameTest;

#include <juce_core/juce_core.h>
#include "../Source/Games/DistortionGame.h"
#include <cmath>

class DistortionGameTest : public juce::UnitTest
{
public:
    DistortionGameTest() : juce::UnitTest ("DistortionGame", "Games") {}

    void runTest() override
    {
        beginTest ("exposes 4 distortion type choices");
        {
            DistortionGame game;
            expectEquals (game.getNumChoices(), DistortionGame::numTypes);
        }

        beginTest ("produces a non-silent buffer for every type without crashing");
        {
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };

            for (int typeIndex = 0; typeIndex < DistortionGame::numTypes; ++typeIndex)
            {
                DistortionGame game;
                game.prepare (spec);

                // Force the round to a specific type by re-rolling until
                // it lands there - simplest way to exercise all four
                // waveshaper branches without adding a test-only setter.
                for (int attempt = 0; attempt < 100 && game.getCorrectChoiceIndex() != typeIndex; ++attempt)
                    game.newRound();

                juce::AudioBuffer<float> buffer (2, 512);
                game.process (buffer);

                float maxAbs = 0.0f;
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        maxAbs = juce::jmax (maxAbs, std::abs (buffer.getSample (ch, i)));

                expect (maxAbs > 0.0f);
            }
        }

        beginTest ("correct answer increases the score");
        {
            DistortionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);

            expect (game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 1);
        }

        beginTest ("wrong answer does not increase the score");
        {
            DistortionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            const auto wrong = (correct + 1) % DistortionGame::numTypes;
            game.submitAnswer (wrong);

            expect (! game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 0);
        }

        beginTest ("a second answer in the same round is ignored");
        {
            DistortionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);
            game.submitAnswer ((correct + 1) % DistortionGame::numTypes);

            expectEquals (game.getRoundsPlayed(), 1);
            expectEquals (game.getChosenChoiceIndex(), correct);
        }

        beginTest ("setDifficulty keeps the same 4 labels and choice count at every tier");
        {
            DistortionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };

            for (const int level : { 1, 5, 10 })
            {
                game.setDifficulty (level);
                game.prepare (spec);

                expectEquals (game.getNumChoices(), DistortionGame::numTypes);
                expectEquals (game.getChoiceLabel (0), juce::String ("Soft Clipping"));
                expectEquals (game.getChoiceLabel (1), juce::String ("Hard Clipping"));
                expectEquals (game.getChoiceLabel (2), juce::String ("Tape Saturation"));
                expectEquals (game.getChoiceLabel (3), juce::String ("Overdrive"));

                const auto correct = game.getCorrectChoiceIndex();
                game.submitAnswer (correct);
                expect (game.wasLastAnswerCorrect());
            }
        }
    }
};

static DistortionGameTest distortionGameTest;

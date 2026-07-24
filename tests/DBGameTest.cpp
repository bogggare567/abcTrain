#include <juce_core/juce_core.h>
#include "../Source/Games/DBGame.h"
#include <cmath>

// DBGame is the one of the five new games where setDifficulty changes the
// choice *labels* themselves (the step size between the five dB deltas
// shrinks at harder tiers), not just an underlying DSP parameter behind
// fixed labels - see the header comment on DBGame for why.
class DBGameTest : public juce::UnitTest
{
public:
    DBGameTest() : juce::UnitTest ("DBGame", "Games") {}

    void runTest() override
    {
        beginTest ("exposes 5 gain-change choices");
        {
            DBGame game;
            expectEquals (game.getNumChoices(), DBGame::numChoices);
        }

        beginTest ("produces a non-silent buffer after newRound()");
        {
            DBGame game;
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
            DBGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);

            expect (game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 1);
        }

        beginTest ("wrong answer does not increase the score");
        {
            DBGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            const auto wrong = (correct + 1) % DBGame::numChoices;
            game.submitAnswer (wrong);

            expect (! game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 0);
        }

        beginTest ("a second answer in the same round is ignored");
        {
            DBGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);
            game.submitAnswer ((correct + 1) % DBGame::numChoices);

            expectEquals (game.getRoundsPlayed(), 1);
            expectEquals (game.getChosenChoiceIndex(), correct);
        }

        beginTest ("setDifficulty keeps 5 choices at every tier, and the medium tier matches the original -6/-3/0/+3/+6 dB spec exactly");
        {
            DBGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };

            game.setDifficulty (1); // easy: +/-6 dB steps
            game.prepare (spec);
            expectEquals (game.getNumChoices(), DBGame::numChoices);
            expectEquals (game.getChoiceLabel (0), juce::String ("-12dB"));
            expectEquals (game.getChoiceLabel (2), juce::String ("0dB"));
            expectEquals (game.getChoiceLabel (4), juce::String ("+12dB"));

            game.setDifficulty (5); // medium: +/-3 dB steps - the literal spec
            game.prepare (spec);
            expectEquals (game.getChoiceLabel (0), juce::String ("-6dB"));
            expectEquals (game.getChoiceLabel (1), juce::String ("-3dB"));
            expectEquals (game.getChoiceLabel (2), juce::String ("0dB"));
            expectEquals (game.getChoiceLabel (3), juce::String ("+3dB"));
            expectEquals (game.getChoiceLabel (4), juce::String ("+6dB"));

            game.setDifficulty (10); // hard: +/-2 dB steps
            game.prepare (spec);
            expectEquals (game.getChoiceLabel (0), juce::String ("-4dB"));
            expectEquals (game.getChoiceLabel (4), juce::String ("+4dB"));

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);
            expect (game.wasLastAnswerCorrect());
        }
    }
};

static DBGameTest dbGameTest;

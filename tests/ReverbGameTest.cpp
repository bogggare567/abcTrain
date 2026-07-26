#include <juce_core/juce_core.h>
#include "../Source/Games/ReverbGame.h"
#include <cmath>

class ReverbGameTest : public juce::UnitTest
{
public:
    ReverbGameTest() : juce::UnitTest ("ReverbGame", "Games") {}

    void runTest() override
    {
        beginTest ("every choice slot has a distinct, non-empty label");
        {
            // A regression guard for a real bug: the choices the UI counts
            // in (0..getNumChoices()-1) and the indices typeLabels uses are
            // two different spaces once types are unlocked in a chosen
            // order rather than in label order. Getting that wrong marks
            // correct answers wrong, silently.
            ReverbGame game;
            game.setDifficulty (10);

            juce::StringArray seen;

            for (int i = 0; i < game.getNumChoices(); ++i)
            {
                const auto label = game.getChoiceLabel (i);
                expect (label.isNotEmpty(), "empty label at slot " + juce::String (i));
                expect (! seen.contains (label), "duplicate label: " + label);
                seen.add (label);
            }
        }

        beginTest ("defaults to the easy tier (2 choices) before setDifficulty is called");
        {
            ReverbGame game;
            expectEquals (game.getNumChoices(), 2);
        }

        beginTest ("setDifficulty unlocks every type at the hard tier");
        {
            ReverbGame game;
            game.setDifficulty (10);
            expectEquals (game.getNumChoices(), ReverbGame::numTypes);
        }

        beginTest ("setDifficulty tiers: 1-2 -> 2, 3-4 -> 3, 5-7 -> 4, 8-10 -> 5");
        {
            ReverbGame game;

            game.setDifficulty (1);
            expectEquals (game.getNumChoices(), 2);

            game.setDifficulty (3);
            expectEquals (game.getNumChoices(), 3);

            game.setDifficulty (5);
            expectEquals (game.getNumChoices(), 4);

            game.setDifficulty (8);
            expectEquals (game.getNumChoices(), 5);
        }

        beginTest ("produces a non-silent buffer after newRound()");
        {
            ReverbGame game;
            game.setDifficulty (10);
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
            ReverbGame game;
            game.setDifficulty (10);
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);

            expect (game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 1);
        }

        beginTest ("wrong answer does not increase the score");
        {
            ReverbGame game;
            game.setDifficulty (10);
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            const auto wrong = (correct + 1) % game.getNumChoices();
            game.submitAnswer (wrong);

            expect (! game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 0);
        }

        beginTest ("a second answer in the same round is ignored");
        {
            ReverbGame game;
            game.setDifficulty (10);
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);
            game.submitAnswer ((correct + 1) % game.getNumChoices());

            expectEquals (game.getRoundsPlayed(), 1);
            expectEquals (game.getChosenChoiceIndex(), correct);
        }
    }
};

static ReverbGameTest reverbGameTest;

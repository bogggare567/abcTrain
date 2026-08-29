#include <juce_core/juce_core.h>
#include <cmath>

#include "../Source/Games/EQGame.h"
#include "../Source/Games/PanGame.h"
#include "../Source/Games/DelayGame.h"
#include "../Source/Games/DBGame.h"
#include "../Source/Games/ReverbGame.h"
#include "../Source/Games/CompressionGame.h"
#include "../Source/Games/DistortionGame.h"
#include "../Source/Games/StereoWidthGame.h"
#include "../Source/Games/FrequencyRangeGame.h"

// What a hint is allowed to do.
//
// The old one opened a live analyser, which on a frequency exercise draws
// the boost as a visible bump - so the answer was read off a picture, and
// in Survival a life was paid for the privilege. The rule this file exists
// to hold is that a hint narrows the search and never resolves it:
//
//   - the answer is inside the region, always;
//   - the region is meaningfully wider than the accept band, so landing
//     inside it is not the same as being right;
//   - the region's centre is not a reliable guess, so nobody can learn to
//     click the middle without listening;
//   - the categorical exercises get nothing, because with two
//     alternatives "narrowing" is just telling you the answer.
class HintTest : public juce::UnitTest
{
public:
    HintTest() : juce::UnitTest ("Hints", "Games") {}

    void runTest() override
    {
        beginTest ("a ruler exercise offers a region, a two-alternative one does not");
        {
            EQGame eq; PanGame pan; DelayGame delay; DBGame db;

            for (Game* game : { (Game*) &eq, (Game*) &pan, (Game*) &delay, (Game*) &db })
            {
                prepare (*game);
                expect (game->getHintHalfWidthNormalised() > 0.0f,
                         juce::String (game->getName()) + " offers no hint region");
            }

            ReverbGame reverb; CompressionGame comp; DistortionGame dist;
            StereoWidthGame width; FrequencyRangeGame range;

            for (Game* game : { (Game*) &reverb, (Game*) &comp, (Game*) &dist,
                                 (Game*) &width, (Game*) &range })
            {
                prepare (*game);
                expectEquals (game->getHintHalfWidthNormalised(), 0.0f,
                               juce::String (game->getName()) + " narrows a scale it does not have");
            }
        }

        beginTest ("the region is wider than the accept band at every level");
        {
            // If it were not, landing inside the shading would be the same
            // as answering correctly, and the hint would be the answer.
            EQGame eq; PanGame pan; DelayGame delay; DBGame db;

            for (Game* game : { (Game*) &eq, (Game*) &pan, (Game*) &delay, (Game*) &db })
            {
                prepare (*game);

                for (int level = 1; level <= 10; ++level)
                {
                    game->setDifficulty (level);
                    game->newRound();

                    const auto band = game->getToleranceNormalised();
                    const auto hint = game->getHintHalfWidthNormalised();

                    expect (hint > band * 2.0f,
                             juce::String (game->getName()) + " at level " + juce::String (level)
                                 + ": region " + juce::String (hint, 4)
                                 + " is not meaningfully wider than the band "
                                 + juce::String (band, 4));
                }
            }
        }

        beginTest ("harder levels get a tighter region, in step with the band");
        {
            EQGame eq;
            prepare (eq);

            eq.setDifficulty (1);
            const auto easy = eq.getHintHalfWidthNormalised();

            eq.setDifficulty (10);
            const auto hard = eq.getHintHalfWidthNormalised();

            expect (hard < easy, "the region did not narrow with the level");

            // The same *relative* help at both ends: always six accept
            // bands of room. That is the property worth holding - a hint
            // that shrank faster or slower than the task would be worth
            // wildly different amounts at different levels.
            eq.setDifficulty (1);
            expectWithinAbsoluteError (eq.getHintHalfWidthNormalised() / eq.getToleranceNormalised(),
                                        3.0f, 0.01f);
            eq.setDifficulty (10);
            expectWithinAbsoluteError (eq.getHintHalfWidthNormalised() / eq.getToleranceNormalised(),
                                        3.0f, 0.01f);
        }

        beginTest ("the answer is always inside the region");
        {
            // Across the whole axis and the whole range of rolls, including
            // both ends, where the clamp that keeps the region on screen
            // could otherwise push it off the answer.
            auto misses = 0;

            for (int a = 0; a <= 100; ++a)
            {
                const auto answer = (float) a / 100.0f;

                for (float halfWidth : { 0.03f, 0.08f, 0.15f, 0.3f, 0.5f })
                {
                    for (int r = 0; r <= 20; ++r)
                    {
                        const auto centre = Game::hintCentreFor (answer, halfWidth, (float) r / 20.0f);

                        if (std::abs (centre - answer) > halfWidth + 1.0e-5f)
                            ++misses;
                    }
                }
            }

            expectEquals (misses, 0, "the region did not contain the answer");
        }

        beginTest ("the centre of the region is not the answer");
        {
            // The one that matters. If the centre tracked the answer, the
            // shading would point straight at it and a player would learn
            // to click the middle rather than to listen.
            constexpr float halfWidth = 0.12f;
            auto sameAsAnswer = 0;
            auto total = 0;

            for (int a = 20; a <= 80; ++a)          // away from the ends, where the clamp bites
            {
                const auto answer = (float) a / 100.0f;

                for (int r = 0; r <= 20; ++r)
                {
                    const auto centre = Game::hintCentreFor (answer, halfWidth, (float) r / 20.0f);
                    ++total;

                    if (std::abs (centre - answer) < halfWidth * 0.02f)
                        ++sameAsAnswer;
                }
            }

            // A roll of exactly 0.5 legitimately lands on the answer; that
            // is one in twenty-one here, so anything near "always" is the
            // failure being described.
            expect (sameAsAnswer * 10 < total,
                     "the region is centred on the answer far too often: "
                         + juce::String (sameAsAnswer) + " of " + juce::String (total));
        }

        beginTest ("the region never leaves the axis");
        {
            auto offAxis = 0;

            for (int a = 0; a <= 100; ++a)
            {
                const auto answer = (float) a / 100.0f;

                for (float halfWidth : { 0.05f, 0.2f, 0.45f })
                {
                    for (int r = 0; r <= 10; ++r)
                    {
                        const auto centre = Game::hintCentreFor (answer, halfWidth, (float) r / 10.0f);

                        if (centre - halfWidth < -1.0e-5f || centre + halfWidth > 1.0f + 1.0e-5f)
                            ++offAxis;
                    }
                }
            }

            expectEquals (offAxis, 0, "the shaded region ran off the end of the ruler");
        }

        beginTest ("a zero-width region is a no-op rather than a divide by zero");
        {
            expectWithinAbsoluteError (Game::hintCentreFor (0.37f, 0.0f, 0.9f), 0.37f, 1.0e-6f);
            expectWithinAbsoluteError (Game::hintCentreFor (0.37f, -1.0f, 0.9f), 0.37f, 1.0e-6f);
        }
    }

private:
    static void prepare (Game& game)
    {
        juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
        game.prepare (spec);
    }
};

static HintTest hintTest;

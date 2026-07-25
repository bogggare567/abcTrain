#include <juce_core/juce_core.h>
#include "../Source/Games/EQGame.h"
#include "../Source/Games/PanGame.h"
#include "../Source/Games/DBGame.h"
#include "../Source/Games/DelayGame.h"
#include <cmath>

// The continuous-answer path added for the four games whose skill is a
// *value* rather than a category. The discrete submitAnswer(int) path is
// still covered by each game's own test file; this covers only what the
// continuous mode introduced - the tolerance band, the axis mapping, and
// the grid ruler.
class ContinuousScaleTest : public juce::UnitTest
{
public:
    ContinuousScaleTest() : juce::UnitTest ("Continuous scales", "Games") {}

    // Every continuous game must agree on the basics, so this runs the
    // same contract against each rather than repeating it four times.
    void checkSharedContract (Game& game, const juce::String& what)
    {
        beginTest (what + ": answering exactly on the target is always correct");
        {
            juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            for (int level : { 1, 5, 10 })
            {
                game.setDifficulty (level);
                game.newRound();

                game.submitNormalisedAnswer (game.getCorrectNormalised());
                expect (game.hasAnswered());
                expect (game.wasLastAnswerCorrect());
            }
        }

        beginTest (what + ": answering a whole axis away is always wrong");
        {
            game.setDifficulty (10);
            game.newRound();

            // Whichever end is further from the target.
            const auto target = game.getCorrectNormalised();
            game.submitNormalisedAnswer (target > 0.5f ? 0.0f : 1.0f);

            expect (game.hasAnswered());
            expect (! game.wasLastAnswerCorrect());
        }

        beginTest (what + ": tolerance narrows as difficulty rises");
        {
            game.setDifficulty (1);
            const auto easy = game.getToleranceNormalised();
            game.setDifficulty (5);
            const auto medium = game.getToleranceNormalised();
            game.setDifficulty (10);
            const auto hard = game.getToleranceNormalised();

            expect (easy > medium);
            expect (medium > hard);
            expect (hard > 0.0f);
        }

        beginTest (what + ": a guess just inside the band passes, just outside fails");
        {
            game.setDifficulty (5);
            game.newRound();

            const auto target = game.getCorrectNormalised();
            const auto tolerance = game.getToleranceNormalised();

            // Aim toward the middle so neither probe falls off the axis.
            const auto direction = target > 0.5f ? -1.0f : 1.0f;

            game.submitNormalisedAnswer (target + direction * tolerance * 0.8f);
            expect (game.wasLastAnswerCorrect());

            game.newRound();
            const auto freshTarget = game.getCorrectNormalised();
            const auto freshDirection = freshTarget > 0.5f ? -1.0f : 1.0f;
            game.submitNormalisedAnswer (freshTarget + freshDirection
                                             * game.getToleranceNormalised() * 2.5f);
            expect (! game.wasLastAnswerCorrect());
        }

        beginTest (what + ": answers after the first are ignored for the round");
        {
            game.newRound();
            const auto roundsBefore = game.getRoundsPlayed();

            game.submitNormalisedAnswer (game.getCorrectNormalised());
            game.submitNormalisedAnswer (0.0f);

            expectEquals (game.getRoundsPlayed(), roundsBefore + 1);
        }

        beginTest (what + ": grid marks are ordered and inside the axis");
        {
            const auto marks = game.getGridMarks();
            expect (marks.size() >= 2);

            auto previous = -1.0f;
            bool sawEmphasised = false;

            for (const auto& mark : marks)
            {
                expect (mark.normalised >= 0.0f && mark.normalised <= 1.0f);
                sawEmphasised = sawEmphasised || mark.emphasised;
                juce::ignoreUnused (previous);
                previous = mark.normalised;
            }

            expect (sawEmphasised);
        }

        beginTest (what + ": the target never lands where it can't be answered");
        {
            // A target closer to an end than the tolerance is unfair: half
            // the accept band would be off the axis.
            game.setDifficulty (1);

            for (int i = 0; i < 40; ++i)
            {
                game.newRound();
                const auto target = game.getCorrectNormalised();
                expect (target >= 0.0f && target <= 1.0f);
            }
        }
    }

    void runTest() override
    {
        {
            EQGame game;
            checkSharedContract (game, "EQ");

            beginTest ("EQ: the axis is logarithmic and covers the labelled range");
            {
                // A whole-axis sweep must pass through every octave centre.
                expectWithinAbsoluteError (EQGame::normalisedToFrequency (
                                                EQGame::frequencyToNormalised (1000.0f)),
                                            1000.0f, 1.0f);

                // Equal ratios take equal distance on the axis: that is what
                // makes an octave-based tolerance mean the same thing
                // everywhere on it.
                const auto lowSpan = EQGame::frequencyToNormalised (400.0f)
                                         - EQGame::frequencyToNormalised (200.0f);
                const auto highSpan = EQGame::frequencyToNormalised (6400.0f)
                                          - EQGame::frequencyToNormalised (3200.0f);
                expectWithinAbsoluteError (lowSpan, highSpan, 0.001f);
            }
        }

        {
            PanGame game;
            checkSharedContract (game, "Pan");

            beginTest ("Pan: centre reads as C, sides as L/R");
            {
                expectEquals (game.formatNormalisedValue (PanGame::panToNormalised (0.0f)),
                              juce::String ("C"));
                expect (game.formatNormalisedValue (PanGame::panToNormalised (-1.0f)).startsWith ("L"));
                expect (game.formatNormalisedValue (PanGame::panToNormalised (1.0f)).startsWith ("R"));
            }
        }

        {
            DBGame game;
            checkSharedContract (game, "dB");

            beginTest ("dB: a boost is signed, and the axis is linear in dB");
            {
                expect (game.formatNormalisedValue (DBGame::dbToNormalised (6.0f)).startsWith ("+"));
                expect (game.formatNormalisedValue (DBGame::dbToNormalised (-6.0f)).startsWith ("-"));

                const auto lowSpan = DBGame::dbToNormalised (-3.0f) - DBGame::dbToNormalised (-6.0f);
                const auto highSpan = DBGame::dbToNormalised (6.0f) - DBGame::dbToNormalised (3.0f);
                expectWithinAbsoluteError (lowSpan, highSpan, 0.001f);
            }
        }

        {
            DelayGame game;
            checkSharedContract (game, "Delay");

            beginTest ("Delay: the tolerance is a ratio, so it is symmetric in log time");
            {
                game.setDifficulty (1);
                game.newRound();

                // 20 ms of error at 40 ms and at 400 ms are wildly
                // different mistakes; a ratio tolerance treats them as
                // such, which a millisecond tolerance would not.
                const auto lowSpan = DelayGame::msToNormalised (80.0f)
                                         - DelayGame::msToNormalised (40.0f);
                const auto highSpan = DelayGame::msToNormalised (640.0f)
                                          - DelayGame::msToNormalised (320.0f);
                expectWithinAbsoluteError (lowSpan, highSpan, 0.001f);
            }
        }
    }
};

static ContinuousScaleTest continuousScaleTest;

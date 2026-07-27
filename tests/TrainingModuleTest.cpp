#include <juce_core/juce_core.h>
#include "../shared/TrainingModule.h"

// The grading rules for a module's check, driven directly. Everything here
// is pure, so none of the message-loop concerns in docs/testing-strategy.md
// apply - and the thing worth protecting is exactly the thing a reading
// cannot confirm: that "within 35%" means the same slack at 3 ms and at
// 300 ms, and that it does not quietly mean something else at the top tier.
class TrainingModuleTest : public juce::UnitTest
{
public:
    TrainingModuleTest() : juce::UnitTest ("TrainingModule") {}

    void runTest() override
    {
        using namespace TrainingModule;

        beginTest ("the band narrows as the tier rises, and stops there");
        {
            Check check;
            check.toleranceAtTierOne = 0.6f;
            check.toleranceAtTopTier = 0.18f;

            const auto first = toleranceForTier (check, 1);
            const auto middle = toleranceForTier (check, 2);
            const auto top = toleranceForTier (check, numTiers);

            expectWithinAbsoluteError (first, 0.6f, 0.001f);
            expectWithinAbsoluteError (top, 0.18f, 0.001f);
            expect (middle < first && middle > top);

            // Out-of-range tiers clamp rather than extrapolating into a
            // band nobody could hit.
            expectWithinAbsoluteError (toleranceForTier (check, 0), first, 0.001f);
            expectWithinAbsoluteError (toleranceForTier (check, 99), top, 0.001f);
        }

        beginTest ("a proportional band is the same slack at every value");
        {
            // The whole reason attack is not graded in milliseconds. A 35%
            // band must accept 4.05 ms against 3 ms and 405 ms against
            // 300 ms, and reject the same overshoot doubled, at both ends.
            Check check;
            check.unit = Unit::proportion;
            check.toleranceAtTierOne = 0.35f;
            check.toleranceAtTopTier = 0.35f;

            expect (passes (check, 3.0f, 3.0f * 1.3f, 1));
            expect (passes (check, 300.0f, 300.0f * 1.3f, 1));
            expect (! passes (check, 3.0f, 3.0f * 1.6f, 1));
            expect (! passes (check, 300.0f, 300.0f * 1.6f, 1));

            // And symmetric: undershooting by the same ratio is the same
            // mistake as overshooting by it.
            expectWithinAbsoluteError (errorFor (check, 100.0f, 200.0f),
                                        errorFor (check, 100.0f, 50.0f), 0.0001f);
        }

        beginTest ("a decibel band is a fixed distance, not a ratio");
        {
            Check check;
            check.unit = Unit::decibels;
            check.toleranceAtTierOne = 2.0f;
            check.toleranceAtTopTier = 2.0f;

            expect (passes (check, -18.0f, -19.5f, 1));
            expect (! passes (check, -18.0f, -21.0f, 1));

            // Negative targets are ordinary here - a threshold is usually
            // one - and must not be treated as a broken ratio.
            expect (passes (check, -40.0f, -38.5f, 1));
        }

        beginTest ("an octave band tracks frequency the way hearing does");
        {
            Check check;
            check.unit = Unit::octaves;
            check.toleranceAtTierOne = 0.5f;
            check.toleranceAtTopTier = 0.5f;

            expect (passes (check, 200.0f, 260.0f, 1));
            expect (passes (check, 6400.0f, 8320.0f, 1));   // the same ratio, eight octaves up
            expect (! passes (check, 200.0f, 400.0f, 1));
        }

        beginTest ("quality is 1 dead on and 0 at the edge");
        {
            Check check;
            check.unit = Unit::decibels;
            check.toleranceAtTierOne = 4.0f;
            check.toleranceAtTopTier = 4.0f;

            expectWithinAbsoluteError (quality (check, -12.0f, -12.0f, 1), 1.0f, 0.001f);
            expectWithinAbsoluteError (quality (check, -12.0f, -14.0f, 1), 0.5f, 0.001f);
            expectEquals (quality (check, -12.0f, -16.0f, 1), 0.0f);

            // Well past the band it stays at zero rather than going
            // negative, so a bonus computed from it can never subtract.
            expectEquals (quality (check, -12.0f, -40.0f, 1), 0.0f);
        }

        beginTest ("a choice check is exact, and its tier changes nothing");
        {
            Check check;
            check.unit = Unit::choice;
            check.toleranceAtTierOne = 0.5f;
            check.toleranceAtTopTier = 0.01f;

            expect (passes (check, 2.0f, 2.0f, 1));
            expect (passes (check, 2.0f, 2.0f, numTiers));
            expect (! passes (check, 2.0f, 3.0f, 1));
            expect (! passes (check, 2.0f, 3.0f, numTiers));
        }

        beginTest ("a zero target under a ratio unit fails rather than blowing up");
        {
            // A module defined with a span starting at zero and a
            // proportional unit is a definition error. It must produce a
            // miss, not an infinity that silently poisons a score.
            Check check;
            check.unit = Unit::proportion;
            check.toleranceAtTierOne = 0.5f;
            check.toleranceAtTopTier = 0.5f;

            expect (! passes (check, 0.0f, 1.0f, 1));
            expectEquals (quality (check, 0.0f, 1.0f, 1), 0.0f);
        }

        beginTest ("drawn targets stay inside the span and honour quantising");
        {
            Check check;
            check.minTarget = 5.0f;
            check.maxTarget = 500.0f;
            check.drawLogarithmically = true;
            check.quantiseTo = 0.5f;

            juce::Random random (1234);
            auto sawSmall = false;
            auto sawLarge = false;

            for (int i = 0; i < 400; ++i)
            {
                const auto value = drawTarget (check, random);

                expect (value >= check.minTarget - 0.001f && value <= check.maxTarget + 0.001f);
                expectWithinAbsoluteError (value - std::round (value / 0.5f) * 0.5f, 0.0f, 0.001f);

                sawSmall = sawSmall || value < 50.0f;
                sawLarge = sawLarge || value > 200.0f;
            }

            // A log draw over 5..500 must actually reach both ends; a
            // uniform draw would put almost nothing below 50.
            expect (sawSmall, "log draw never produced a small target");
            expect (sawLarge, "log draw never produced a large target");
        }

        beginTest ("the same seed draws the same round");
        {
            Check check;
            check.minTarget = 1.0f;
            check.maxTarget = 100.0f;

            juce::Random a (77), b (77);
            expectEquals (drawTarget (check, a), drawTarget (check, b));
        }
    }
};

static TrainingModuleTest trainingModuleTest;

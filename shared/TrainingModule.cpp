#include "TrainingModule.h"
#include "DifficultyRamp.h"
#include <cmath>

namespace TrainingModule
{
    namespace
    {
        // Ratio-based units are undefined at zero, and a target of zero
        // under one of them means the module was defined wrong. Return the
        // widest error the check can produce rather than an infinity that
        // would propagate silently through a quality score.
        constexpr float unusableError = 1.0e6f;

        float safeRatio (float target, float answer) noexcept
        {
            if (target <= 0.0f || answer <= 0.0f)
                return 0.0f;

            return answer / target;
        }
    }

    float toleranceForTier (const Check& check, int tier) noexcept
    {
        // The tier ladder is 1..3 but the ramp helper speaks 1..10, so map
        // the ends onto it. Using the helper rather than a local lerp is
        // deliberate: an accept band that narrows differently here than in
        // the trainer would make "tier 3" and "level 10" incomparable.
        const auto clamped = juce::jlimit (1, numTiers, tier);
        const auto asTenPointScale = 1 + (clamped - 1) * 9 / (numTiers - 1);

        return DifficultyRamp::geometric (asTenPointScale, check.toleranceAtTierOne,
                                           check.toleranceAtTopTier);
    }

    float errorFor (const Check& check, float target, float answer) noexcept
    {
        switch (check.unit)
        {
            case Unit::decibels:
                return std::abs (answer - target);

            case Unit::proportion:
            {
                const auto ratio = safeRatio (target, answer);
                return ratio > 0.0f ? std::abs (std::log (ratio)) : unusableError;
            }

            case Unit::octaves:
            {
                const auto ratio = safeRatio (target, answer);
                return ratio > 0.0f ? std::abs (std::log2 (ratio)) : unusableError;
            }

            case Unit::rangeFraction:
            {
                const auto span = check.maxTarget - check.minTarget;
                return span > 0.0f ? std::abs (answer - target) / span : unusableError;
            }

            case Unit::choice:
                return std::abs (answer - target) < 0.5f ? 0.0f : 1.0f;
        }

        return unusableError;
    }

    // Internal: the accept band expressed in whatever errorFor returns.
    static float toleranceInErrorUnits (const Check& check, int tier) noexcept
    {
        const auto tolerance = toleranceForTier (check, tier);

        // A proportional tolerance is written the way a person says it -
        // "within 35%" - but the error is a log ratio, so the band has to
        // be converted rather than compared directly. ln(1.35), not 0.35.
        if (check.unit == Unit::proportion)
            return std::log (1.0f + tolerance);

        return tolerance;
    }

    bool passes (const Check& check, float target, float answer, int tier) noexcept
    {
        if (check.unit == Unit::choice)
            return errorFor (check, target, answer) < 0.5f;

        return errorFor (check, target, answer) <= toleranceInErrorUnits (check, tier);
    }

    float quality (const Check& check, float target, float answer, int tier) noexcept
    {
        if (check.unit == Unit::choice)
            return passes (check, target, answer, tier) ? 1.0f : 0.0f;

        const auto band = toleranceInErrorUnits (check, tier);

        if (band <= 0.0f)
            return 0.0f;

        return juce::jlimit (0.0f, 1.0f, 1.0f - errorFor (check, target, answer) / band);
    }

    float drawTarget (const Check& check, juce::Random& random) noexcept
    {
        const auto low = juce::jmin (check.minTarget, check.maxTarget);
        const auto high = juce::jmax (check.minTarget, check.maxTarget);

        if (high <= low)
            return low;

        auto value = low;

        if (check.drawLogarithmically && low > 0.0f)
        {
            const auto t = (float) random.nextDouble();
            value = low * std::pow (high / low, t);
        }
        else
        {
            value = low + (float) random.nextDouble() * (high - low);
        }

        if (check.quantiseTo > 0.0f)
            value = std::round (value / check.quantiseTo) * check.quantiseTo;

        return juce::jlimit (low, high, value);
    }
}

#include "shared/learning/TrainingModule.h"
#include "shared/learning/DifficultyRamp.h"
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

    float toleranceForLevel (const Check& check, int level) noexcept
    {
        return DifficultyRamp::geometric (juce::jlimit (1, maxLevel, level),
                                           check.toleranceAtTierOne, check.toleranceAtTopTier);
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
    static float bandInErrorUnits (const Check& check, float tolerance) noexcept
    {
        // A proportional tolerance is written the way a person says it -
        // "within 35%" - but the error is a log ratio, so the band has to
        // be converted rather than compared directly. ln(1.35), not 0.35.
        if (check.unit == Unit::proportion)
            return std::log (1.0f + tolerance);

        return tolerance;
    }

    static float toleranceInErrorUnits (const Check& check, int tier) noexcept
    {
        return bandInErrorUnits (check, toleranceForTier (check, tier));
    }

    bool passesAtLevel (const Check& check, float target, float answer, int level) noexcept
    {
        if (check.unit == Unit::choice)
            return errorFor (check, target, answer) < 0.5f;

        return errorFor (check, target, answer) <= bandInErrorUnits (check, toleranceForLevel (check, level));
    }

    float qualityAtLevel (const Check& check, float target, float answer, int level) noexcept
    {
        if (check.unit == Unit::choice)
            return passesAtLevel (check, target, answer, level) ? 1.0f : 0.0f;

        const auto band = bandInErrorUnits (check, toleranceForLevel (check, level));

        if (band <= 0.0f)
            return 0.0f;

        return juce::jlimit (0.0f, 1.0f, 1.0f - errorFor (check, target, answer) / band);
    }

    juce::Range<float> acceptRange (const Check& check, float value, int level) noexcept
    {
        const auto tolerance = toleranceForLevel (check, level);

        switch (check.unit)
        {
            case Unit::decibels:      return { value - tolerance, value + tolerance };
            case Unit::proportion:    return { value / (1.0f + tolerance), value * (1.0f + tolerance) };
            case Unit::octaves:       return { value / std::pow (2.0f, tolerance), value * std::pow (2.0f, tolerance) };
            case Unit::rangeFraction:
            {
                const auto span = std::abs (check.maxTarget - check.minTarget) * tolerance;
                return { value - span, value + span };
            }
            case Unit::choice:        return { value, value };
        }

        return { value, value };
    }

    Readout readout (const Check& check, float target, float answer, int level) noexcept
    {
        Readout r;
        const auto tolerance = toleranceForLevel (check, level);

        switch (check.unit)
        {
            case Unit::decibels:
                r.tolerance = tolerance;
                r.signedError = answer - target;
                break;

            case Unit::proportion:
                r.percent = true;
                r.tolerance = tolerance * 100.0f;
                r.signedError = target > 0.0f ? (answer / target - 1.0f) * 100.0f : 0.0f;
                break;

            case Unit::octaves:
                r.octaves = true;
                r.tolerance = tolerance;
                r.signedError = (target > 0.0f && answer > 0.0f) ? std::log2 (answer / target) : 0.0f;
                break;

            case Unit::rangeFraction:
                r.tolerance = std::abs (check.maxTarget - check.minTarget) * tolerance * check.displayScale;
                r.signedError = (answer - target) * check.displayScale;
                break;

            case Unit::choice:
                break;
        }

        return r;
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

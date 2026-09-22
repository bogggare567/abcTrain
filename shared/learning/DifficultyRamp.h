#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

// How a number moves as difficulty rises, in the two shapes this project
// needs. Extracted from Game.h once the Learner plugins' training modules
// wanted the same ramps: a module's accept band has to narrow exactly the
// way an exercise's does, or "level 3" means two different things in two
// halves of the same product.
namespace DifficultyRamp
{
    // Geometric. For anything that is a *tolerance* or any other quantity
    // where equal ratios matter and zero is not reachable - milliseconds,
    // octaves, a percentage of a value. Halving twice is the same step
    // twice; subtracting a fixed amount twice is not.
    inline float geometric (int level, float atLevelOne, float atLevelTen) noexcept
    {
        const auto t = (float) (juce::jlimit (1, 10, level) - 1) / 9.0f;
        return atLevelOne * std::pow (atLevelTen / atLevelOne, t);
    }

    // Linear. For a value that can legitimately pass through zero or change
    // sign, where the geometric ramp is undefined.
    inline float linear (int level, float atLevelOne, float atLevelTen) noexcept
    {
        const auto t = (float) (juce::jlimit (1, 10, level) - 1) / 9.0f;
        return atLevelOne + (atLevelTen - atLevelOne) * t;
    }
}

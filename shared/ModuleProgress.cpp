#include "ModuleProgress.h"

ModuleProgress::State ModuleProgress::get (const juce::String& moduleId) const
{
    State s;

    if (properties.containsKey (key (moduleId, "level")))
    {
        s.level = juce::jlimit (1, maxLevel, properties.getIntValue (key (moduleId, "level"), 1));
        s.bestLevel = juce::jlimit (s.level, maxLevel, properties.getIntValue (key (moduleId, "best"), s.level));
        s.stepRun = juce::jlimit (0, stepUpAfter - 1, properties.getIntValue (key (moduleId, "run"), 0));
        s.attempts = juce::jmax (0, properties.getIntValue (key (moduleId, "attempts"), 0));
        s.passes = juce::jlimit (0, s.attempts, properties.getIntValue (key (moduleId, "passes"), 0));
        return s;
    }

    // A tier from the old three-step ladder.
    const auto tier = juce::jlimit (0, 3, properties.getIntValue (key (moduleId, "tier"), 0));

    if (tier > 0)
        s.level = s.bestLevel = 1 + tier * 3;

    return s;
}

ModuleProgress::Outcome ModuleProgress::recordAttempt (const juce::String& moduleId, bool passed)
{
    auto s = get (moduleId);
    const auto before = s.level;

    Outcome outcome;
    outcome.passed = passed;

    ++s.attempts;

    if (passed)
    {
        ++s.passes;

        if (++s.stepRun >= stepUpAfter)
        {
            if (s.level < maxLevel)
            {
                ++s.level;
                s.stepRun = 0;
            }
            else
            {
                s.stepRun = stepUpAfter - 1;
            }
        }
    }
    else
    {
        s.stepRun = 0;
        s.level = juce::jmax (1, s.level - 1);
    }

    if (s.level > s.bestLevel)
    {
        s.bestLevel = s.level;
        outcome.newBest = true;
    }

    outcome.steppedUp = s.level > before;
    outcome.steppedDown = s.level < before;
    outcome.after = s;

    properties.setValue (key (moduleId, "level"), s.level);
    properties.setValue (key (moduleId, "best"), s.bestLevel);
    properties.setValue (key (moduleId, "run"), s.stepRun);
    properties.setValue (key (moduleId, "attempts"), s.attempts);
    properties.setValue (key (moduleId, "passes"), s.passes);
    properties.saveIfNeeded();

    return outcome;
}

int ModuleProgress::getTierPassed (const juce::String& moduleId) const
{
    const auto best = get (moduleId).bestLevel;
    return best >= 10 ? 3 : best >= 7 ? 2 : best >= 4 ? 1 : 0;
}

bool ModuleProgress::hasSeenDemo (const juce::String& moduleId) const
{
    return properties.getBoolValue (key (moduleId, "demoSeen"), false);
}

void ModuleProgress::markDemoSeen (const juce::String& moduleId)
{
    properties.setValue (key (moduleId, "demoSeen"), true);
    properties.saveIfNeeded();
}

#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_core/juce_core.h>

// Where the player stands on each training module, persisted.
//
// **A staircase per module, the trainer's own (ADR 035, ADR 037).** Three
// checks passed in a row take one step harder - a narrower accept band -
// one missed takes one step easier. Two numbers per module: `level`, where
// the staircase stands now, and `bestLevel`, the record, which never drops
// and is what screens lead with. It replaced three tiers that only ever
// rose: one lucky pass promoted a module for good, and a miss was not
// recorded at all, so the number said how often you had tried rather than
// how precisely you hear.
//
// Keyed by module id string, never by index, so reordering the modules in
// a plugin never shuffles anyone's progress. A tier saved by the old ladder
// becomes both the starting step and the record (tier 1 -> 4, 2 -> 7,
// 3 -> 10), so nobody who upgrades finds a module reset to the bottom.
class ModuleProgress
{
public:
    explicit ModuleProgress (juce::PropertiesFile& propertiesFile) : properties (propertiesFile) {}

    struct State
    {
        int level = 1;
        int bestLevel = 1;
        int stepRun = 0;       // passes in a row toward the next step
        int attempts = 0;
        int passes = 0;
    };

    State get (const juce::String& moduleId) const;

    struct Outcome
    {
        bool passed = false;
        bool steppedUp = false;
        bool steppedDown = false;
        bool newBest = false;
        State after;
    };

    // One graded check. Moves the staircase and saves.
    Outcome recordAttempt (const juce::String& moduleId, bool passed);

    bool hasSeenDemo (const juce::String& moduleId) const;
    void markDemoSeen (const juce::String& moduleId);

    static constexpr int stepUpAfter = 3;
    static constexpr int maxLevel = 10;

    // The old API, kept so saved data and tests still read: the tier the
    // record corresponds to (step 4 = tier 1, 7 = 2, 10 = 3).
    int getTierPassed (const juce::String& moduleId) const;

private:
    juce::String key (const juce::String& moduleId, const char* field) const
    {
        return "module." + moduleId + "." + field;
    }

    juce::PropertiesFile& properties;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleProgress)
};

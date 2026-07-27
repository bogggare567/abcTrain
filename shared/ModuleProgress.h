#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_core/juce_core.h>

// What a player has got through in the Learner plugins' training modules.
//
// Kept in the shared "abcTrain" PropertiesFile alongside theme, language
// and the practice-audio choice - not in any plugin's APVTS. What you have
// learned is a property of you, not of a saved project, and it should be
// the same whether you open Learner EQ in one host or another.
//
// Keyed by the module's string id rather than by index, deliberately. Ear
// Trainer's per-exercise stats are index-keyed and that has been a standing
// hazard ever since (a new exercise must be appended, never inserted, and
// nothing in the code enforces it). Modules will be reordered as more are
// written; a string key makes reordering free.
//
// No Component, no message loop: tests drive it against a temporary
// PropertiesFile directly.
class ModuleProgress
{
public:
    explicit ModuleProgress (juce::PropertiesFile& propertiesFile) : properties (propertiesFile) {}

    // The highest tier this module has been passed at, 0 if never. Tiers
    // are 1..TrainingModule::numTiers.
    int getTierPassed (const juce::String& moduleId) const;

    // Records a completed check. Only ever raises the stored tier: a bad
    // run at tier 3 does not take away a tier 2 you already earned, for the
    // same reason earned achievements are never removed.
    void recordPass (const juce::String& moduleId, int tier);

    // The demonstration having been watched is worth remembering separately
    // from the check having been passed - it is the difference between
    // "seen" and "can do", and the shelf shows them differently.
    bool hasSeenDemo (const juce::String& moduleId) const;
    void markDemoSeen (const juce::String& moduleId);

    // 0..1 across a set of module ids, counting tiers rather than modules,
    // so getting one module to the top tier is not the same as scraping
    // three of them at tier 1.
    float completionFor (const juce::StringArray& moduleIds) const;

private:
    juce::String tierKeyFor (const juce::String& moduleId) const;
    juce::String demoKeyFor (const juce::String& moduleId) const;

    juce::PropertiesFile& properties;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleProgress)
};

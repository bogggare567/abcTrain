#include "ModuleProgress.h"
#include "TrainingModule.h"

juce::String ModuleProgress::tierKeyFor (const juce::String& moduleId) const
{
    return "module." + moduleId + ".tier";
}

juce::String ModuleProgress::demoKeyFor (const juce::String& moduleId) const
{
    return "module." + moduleId + ".demoSeen";
}

int ModuleProgress::getTierPassed (const juce::String& moduleId) const
{
    return juce::jlimit (0, TrainingModule::numTiers,
                          properties.getIntValue (tierKeyFor (moduleId), 0));
}

void ModuleProgress::recordPass (const juce::String& moduleId, int tier)
{
    const auto clamped = juce::jlimit (1, TrainingModule::numTiers, tier);

    if (clamped <= getTierPassed (moduleId))
        return;

    properties.setValue (tierKeyFor (moduleId), clamped);
    properties.saveIfNeeded();
}

bool ModuleProgress::hasSeenDemo (const juce::String& moduleId) const
{
    return properties.getBoolValue (demoKeyFor (moduleId), false);
}

void ModuleProgress::markDemoSeen (const juce::String& moduleId)
{
    properties.setValue (demoKeyFor (moduleId), true);
    properties.saveIfNeeded();
}

float ModuleProgress::completionFor (const juce::StringArray& moduleIds) const
{
    if (moduleIds.isEmpty())
        return 0.0f;

    auto earned = 0;

    for (const auto& id : moduleIds)
        earned += getTierPassed (id);

    return (float) earned / (float) (moduleIds.size() * TrainingModule::numTiers);
}

// Prints every piece of English text the three Learner plugins show - module
// and walkthrough content, parameter guides, presets, EQ zones - as the
// i18n keys the editors look them up by, so a translation starts from the
// real strings rather than from a copy that has already drifted.
//
//   LearnerStrings > learner-en.json
//
// The editors fall back to this English when a language has no entry, so a
// key missing from a table is an untranslated sentence, never a blank.
#include <juce_core/juce_core.h>
#include "../LearnerComp/Source/CompressorModules.h"
#include "../LearnerComp/Source/ParameterGuide.h"
#include "../LearnerVerb/Source/ReverbModules.h"
#include "../LearnerVerb/Source/ReverbGuide.h"
#include "../LearnerEQ/Source/EQModules.h"
#include "../LearnerEQ/Source/FrequencyGuide.h"
#include "../LearnerEQ/Source/FrequencyZones.h"
#include <iostream>
#include <map>

int main()
{
    std::map<juce::String, juce::String> out;

    const auto addModules = [&out] (const std::vector<TrainingModule::Definition>& modules)
    {
        for (const auto& m : modules)
        {
            out["mod." + m.id + ".name"] = m.name;
            if (m.why.isNotEmpty())       out["mod." + m.id + ".why"] = m.why;
            if (m.tryPrompt.isNotEmpty()) out["mod." + m.id + ".try"] = m.tryPrompt;

            for (size_t i = 0; i < m.demoSteps.size(); ++i)
                out["mod." + m.id + ".step" + juce::String ((int) i + 1)] = m.demoSteps[i].explanationText;
        }
    };

    addModules (CompressorModules::all());
    addModules (CompressorModules::walkthroughs());
    addModules (ReverbModules::all());
    addModules (ReverbModules::walkthroughs());
    addModules (EQModules::all());
    addModules (EQModules::walkthroughs());

    for (auto id : { "threshold", "ratio", "attack", "release", "knee", "makeup", "dryWet" })
        out["guide.comp." + juce::String (id)] = CompressorGuide::describe (id);

    for (auto id : { "type", "decay", "preDelay", "size", "damping", "dryWet", "width" })
        out["guide.verb." + juce::String (id)] = ReverbGuide::describe (id);

    for (float f : { 100.0f, 200.0f, 600.0f, 2000.0f, 4000.0f, 8000.0f, 15000.0f })
        out["guide.eq.freq." + juce::String (FrequencyGuide::rangeIndexFor (f))] = FrequencyGuide::describe (f);

    for (size_t i = 0; i < CompressorGuide::presets.size(); ++i)
    {
        out["preset.comp." + juce::String ((int) i) + ".name"] = CompressorGuide::presets[i].name;
        out["preset.comp." + juce::String ((int) i) + ".what"] = CompressorGuide::presets[i].what;
    }

    for (size_t i = 0; i < ReverbGuide::presets.size(); ++i)
    {
        out["preset.verb." + juce::String ((int) i) + ".name"] = ReverbGuide::presets[i].name;
        out["preset.verb." + juce::String ((int) i) + ".what"] = ReverbGuide::presets[i].what;
    }

    for (const auto& zone : FrequencyZones::all)
    {
        out["zone." + juce::String (zone.name) + ".name"] = zone.name;
        out["zone." + juce::String (zone.name) + ".feels"] = zone.feels;
    }

    auto* json = new juce::DynamicObject();

    for (const auto& [key, value] : out)
        json->setProperty (juce::Identifier (key), value);

    std::cout << juce::JSON::toString (juce::var (json), false).toStdString() << std::endl;
    return 0;
}

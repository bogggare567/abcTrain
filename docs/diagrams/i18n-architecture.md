# i18n architecture

How a language choice turns into visible text in the editor. See
[decisions/011-i18n.md](../decisions/011-i18n.md) for the full rationale —
this is the shape, not the reasoning.

```mermaid
flowchart TB
    subgraph BuildTime["Build time"]
        JsonFiles["shared/i18n/strings/*.json\n(en, ru, de, fr, es, pt,\nzh-Hans, ja, ko, it, pl, uk)"]
        BinaryDataGen["juce_add_binary_data(I18nData)\n(CMakeLists.txt)"]
        BinaryDataOut["BinaryData::en_json / ::ru_json / ...\n(generated .cpp/.h)"]

        JsonFiles --> BinaryDataGen --> BinaryDataOut
    end

    subgraph Runtime["Runtime (each plugin instance)"]
        SystemLang["juce::SystemStats::getUserLanguage()"]
        PropsFile[("juce::PropertiesFile\n(shared 'abcTrain' folder)")]
        LM["LocalisationManager"]
        GetBinaryData["getBinaryDataFor(code)\n(explicit code -> symbol map)"]
        JsonParse["juce::JSON::parse\n(juce::String::fromUTF8)"]
        Table[("in-memory table\nfor current + English\n(loadedTables cache)")]

        SystemLang -- "first run only" --> LM
        PropsFile <-- "load/save 'language'" --> LM
        LM --> GetBinaryData --> BinaryDataOut
        GetBinaryData --> JsonParse --> Table
    end

    subgraph Editor["EarTrainerEditor (reference integration)"]
        LangCombo["language ComboBox"]
        GetText["localisation.getText(key)\nlocalisation.getText(key, placeholders)"]
        NameMap["englishName -> i18n key\n(translateGameName/\ntranslateGameInstructions)"]
        Labels["titleLabel, updateButton,\ngameSelector items,\ninstructionLabel, scoreLabel,\nlevelLabel, streakLabel"]

        LangCombo -- "onChange: setLanguage(code)" --> LM
        LM -- "sendChangeMessage()" -.-> Editor
        LangCombo -- "languageSelected():\nrefreshLocalisedText() (direct call,\nnot the async ChangeListener path)" --> GetText
        Table --> GetText
        NameMap --> GetText
        GetText --> Labels
    end

    subgraph NotYetWired["Not wired up yet"]
        OtherEditors["LearnerEQ / LearnerComp /\nLearnerVerb editors\n(no language ComboBox)"]
        Tooltips["ParameterGuide.h / ReverbGuide.h /\nFrequencyGuide.h tooltip text\n(still English-only)"]
    end

    classDef planned stroke-dasharray:4 3,opacity:0.55;
    class NotYetWired,OtherEditors,Tooltips planned;
```

**Why `getText` falls back English → the literal key**: a missing
translation should be *visible* (a stray `"some.key"` string in the UI)
rather than silently blank — much easier to spot and fix.

**Why the language choice is one shared `PropertiesFile`, not
per-plugin**: `LocalisationManager::makeDefaultOptions()` points every
plugin instance at the same `"abcTrain"` folder, so picking a language in
one plugin doesn't require picking it again in the other three (once they
get their own selector).

**Why `BinaryData`, not reading the JSON files from disk at runtime**: a
plugin binary's install location differs across VST3/AU/Standalone and
across OSes - there's no single reliable relative path from "wherever this
`.vst3`/`.component`/executable ended up" back to a `strings/` folder.
Embedding the JSON at build time sidesteps that entirely.

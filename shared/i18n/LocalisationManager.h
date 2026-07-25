#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <memory>
#include <map>

// Minimal JSON-backed i18n: one flat string table per supported language,
// embedded into the binary via BinaryData (see shared/i18n/strings/*.json
// and the juce_add_binary_data(I18nData ...) target in CMakeLists.txt) so
// lookup never depends on a filesystem path relative to the plugin binary.
//
// Deliberately scoped: covers the player-visible core UI strings (game
// names/instructions, common buttons/labels) across all 12 supported
// languages. Full tooltip/lesson-step text is still English-only - see
// docs/roadmap.md and decisions/011-i18n.md for what's covered now vs.
// deferred.
class LocalisationManager : public juce::ChangeBroadcaster
{
public:
    // Codes match the JSON filenames under shared/i18n/strings/ (dashes
    // replaced with underscores in the embedded BinaryData symbol name,
    // e.g. "zh-Hans" -> BinaryData::zhHans_json).
    static const juce::StringArray& getSupportedLanguageCodes();

    // Human-readable name for a language code, in that language's own
    // script (for a language picker) - e.g. "ru" -> "Русский".
    static juce::String getDisplayName (const juce::String& languageCode);

    // One shared PropertiesFile folder ("abcTrain", not per-plugin) so
    // the language choice is a single product-wide preference rather
    // than something you have to set separately in each of the four
    // plugins.
    static juce::PropertiesFile::Options makeDefaultOptions();

    // Loads the persisted choice from PropertiesFile if one exists;
    // otherwise auto-detects from juce::SystemStats::getUserLanguage()
    // (falling back to "en" if the system language isn't supported) and
    // persists that as the initial choice.
    explicit LocalisationManager (juce::PropertiesFile& propertiesFile);

    // No-op (and does not notify) if languageCode isn't supported or is
    // already current. Persists the choice and calls sendChangeMessage()
    // otherwise, so editors listening via ChangeListener can refresh
    // every visible string immediately.
    void setLanguage (const juce::String& languageCode);
    juce::String getCurrentLanguage() const { return currentLanguage; }

    // Looks up `key` in the current language's table, falling back to
    // English (and then to the key itself, so a missing translation is
    // visibly obvious rather than blank) if not found.
    juce::String getText (const juce::String& key) const;

    // Same lookup, then replaces every "{{name}}" placeholder with the
    // matching value from `placeholders` (e.g. getText("ui.level",
    // {{"level", "5"}}) -> "Level 5").
    juce::String getText (const juce::String& key, const std::map<juce::String, juce::String>& placeholders) const;

private:
    static juce::var loadLanguageTable (const juce::String& languageCode);

    juce::PropertiesFile& properties;
    juce::String currentLanguage;
    std::map<juce::String, juce::var> loadedTables;

    const juce::var& getTable (const juce::String& languageCode) const;
};

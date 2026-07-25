#include "LocalisationManager.h"
#include "BinaryData.h"

namespace
{
    constexpr const char* propertiesKey = "language";

    // language code -> (BinaryData bytes, size). BinaryData's generator
    // sanitises "zh-Hans.json" to the symbol "zhHans_json" (the dash is
    // dropped, not turned into an underscore, while the extension's dot
    // becomes one) - not a predictable-enough transform to build
    // programmatically from the code string, hence the explicit table.
    bool getBinaryDataFor (const juce::String& languageCode, const char*& data, int& size)
    {
        if (languageCode == "en")      { data = BinaryData::en_json;      size = BinaryData::en_jsonSize;      return true; }
        if (languageCode == "ru")      { data = BinaryData::ru_json;      size = BinaryData::ru_jsonSize;      return true; }
        if (languageCode == "de")      { data = BinaryData::de_json;      size = BinaryData::de_jsonSize;      return true; }
        if (languageCode == "fr")      { data = BinaryData::fr_json;      size = BinaryData::fr_jsonSize;      return true; }
        if (languageCode == "es")      { data = BinaryData::es_json;      size = BinaryData::es_jsonSize;      return true; }
        if (languageCode == "pt")      { data = BinaryData::pt_json;      size = BinaryData::pt_jsonSize;      return true; }
        if (languageCode == "zh-Hans") { data = BinaryData::zhHans_json; size = BinaryData::zhHans_jsonSize; return true; }
        if (languageCode == "ja")      { data = BinaryData::ja_json;      size = BinaryData::ja_jsonSize;      return true; }
        if (languageCode == "ko")      { data = BinaryData::ko_json;      size = BinaryData::ko_jsonSize;      return true; }
        if (languageCode == "it")      { data = BinaryData::it_json;      size = BinaryData::it_jsonSize;      return true; }
        if (languageCode == "pl")      { data = BinaryData::pl_json;      size = BinaryData::pl_jsonSize;      return true; }
        if (languageCode == "uk")      { data = BinaryData::uk_json;      size = BinaryData::uk_jsonSize;      return true; }
        return false;
    }

    juce::String detectSystemLanguage (const juce::StringArray& supported)
    {
        // SystemStats::getUserLanguage() returns an ISO 639 code, possibly
        // with a region suffix (e.g. "en-GB") - only the primary subtag
        // matters for our table selection.
        const auto full = juce::SystemStats::getUserLanguage();
        const auto primary = full.upToFirstOccurrenceOf ("-", false, false).toLowerCase();

        if (primary == "zh")
            return "zh-Hans";

        if (supported.contains (primary))
            return primary;

        return "en";
    }
}

const juce::StringArray& LocalisationManager::getSupportedLanguageCodes()
{
    static const juce::StringArray codes {
        "en", "ru", "de", "fr", "es", "pt", "zh-Hans", "ja", "ko", "it", "pl", "uk"
    };
    return codes;
}

juce::String LocalisationManager::getDisplayName (const juce::String& languageCode)
{
    // juce::String's plain `const char*` constructor does NOT assume
    // UTF-8 (it's a known JUCE gotcha - non-ASCII literals silently
    // mojibake without this), so every non-ASCII name below is wrapped
    // in CharPointer_UTF8 explicitly. This source file itself is saved
    // as UTF-8, so the literals' bytes are correct; CharPointer_UTF8 is
    // what tells JUCE to actually decode them as such.
    if (languageCode == "en")      return "English";
    if (languageCode == "ru")      return juce::CharPointer_UTF8 ("Русский");
    if (languageCode == "de")      return "Deutsch";
    if (languageCode == "fr")      return juce::CharPointer_UTF8 ("Français");
    if (languageCode == "es")      return juce::CharPointer_UTF8 ("Español");
    if (languageCode == "pt")      return juce::CharPointer_UTF8 ("Português");
    if (languageCode == "zh-Hans") return juce::CharPointer_UTF8 ("简体中文");
    if (languageCode == "ja")      return juce::CharPointer_UTF8 ("日本語");
    if (languageCode == "ko")      return juce::CharPointer_UTF8 ("한국어");
    if (languageCode == "it")      return "Italiano";
    if (languageCode == "pl")      return "Polski";
    if (languageCode == "uk")      return juce::CharPointer_UTF8 ("Українська");
    return languageCode;
}

juce::PropertiesFile::Options LocalisationManager::makeDefaultOptions()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "abcTrain";
    options.filenameSuffix = "settings";
    options.folderName = "abcTrain";
    options.osxLibrarySubFolder = "Application Support";
    return options;
}

LocalisationManager::LocalisationManager (juce::PropertiesFile& propertiesFile)
    : properties (propertiesFile)
{
    const auto saved = properties.getValue (propertiesKey, {});
    const auto& supported = getSupportedLanguageCodes();

    if (saved.isNotEmpty() && supported.contains (saved))
    {
        currentLanguage = saved;
    }
    else
    {
        currentLanguage = detectSystemLanguage (supported);
        properties.setValue (propertiesKey, currentLanguage);
    }
}

void LocalisationManager::setLanguage (const juce::String& languageCode)
{
    if (! getSupportedLanguageCodes().contains (languageCode) || languageCode == currentLanguage)
        return;

    currentLanguage = languageCode;
    properties.setValue (propertiesKey, currentLanguage);
    sendChangeMessage();
}

juce::var LocalisationManager::loadLanguageTable (const juce::String& languageCode)
{
    const char* data = nullptr;
    int size = 0;

    if (! getBinaryDataFor (languageCode, data, size))
        return {};

    return juce::JSON::parse (juce::String::fromUTF8 (data, size));
}

const juce::var& LocalisationManager::getTable (const juce::String& languageCode) const
{
    auto it = loadedTables.find (languageCode);
    if (it != loadedTables.end())
        return it->second;

    auto& mutableSelf = const_cast<LocalisationManager&> (*this);
    auto result = mutableSelf.loadedTables.emplace (languageCode, loadLanguageTable (languageCode));
    return result.first->second;
}

juce::String LocalisationManager::getText (const juce::String& key) const
{
    const auto& currentTable = getTable (currentLanguage);
    if (const auto* obj = currentTable.getDynamicObject())
    {
        if (obj->hasProperty (key))
            return obj->getProperty (key).toString();
    }

    if (currentLanguage != "en")
    {
        const auto& englishTable = getTable ("en");
        if (const auto* obj = englishTable.getDynamicObject())
        {
            if (obj->hasProperty (key))
                return obj->getProperty (key).toString();
        }
    }

    return key;
}

juce::String LocalisationManager::getText (const juce::String& key, const std::map<juce::String, juce::String>& placeholders) const
{
    auto text = getText (key);
    for (const auto& [name, value] : placeholders)
        text = text.replace ("{{" + name + "}}", value);
    return text;
}

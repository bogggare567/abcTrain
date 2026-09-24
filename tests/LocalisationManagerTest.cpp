#include <juce_core/juce_core.h>
#include "shared/i18n/LocalisationManager.h"
#include "BinaryData.h"

class LocalisationManagerTest : public juce::UnitTest
{
public:
    LocalisationManagerTest() : juce::UnitTest ("LocalisationManager", "Shared") {}

    // Same pattern as ProgressManagerTest's makeTempOptions: a unique
    // folder per test plus deleteFile() up front, so leftover
    // PropertiesFile state from a previous local run of this binary
    // never leaks into a test that assumes a fresh file (CI always runs
    // in a fresh container, so this only ever bit local re-runs).
    static juce::PropertiesFile::Options makeOptions (const juce::String& uniqueSuffix)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "EarTrainerTests";
        options.filenameSuffix = "settings";
        options.folderName = "EarTrainerTests_LocalisationManager_" + uniqueSuffix;
        options.commonToAllUsers = false;

        options.getDefaultFile().deleteFile();

        return options;
    }

    void runTest() override
    {
        beginTest ("all 12 supported languages load a non-empty table");
        {
            for (const auto& code : LocalisationManager::getSupportedLanguageCodes())
            {
                juce::PropertiesFile props (makeOptions ("load_" + code));
                props.setValue ("language", code);
                LocalisationManager manager (props);

                expectEquals (manager.getCurrentLanguage(), code);
                expect (manager.getText ("ui.close").isNotEmpty());
                expect (manager.getText ("game.eq.name").isNotEmpty());
            }
        }

        beginTest ("every language table has every English key, and no extra ones");
        {
            // getText falls back to English, so a missing translation is
            // invisible at runtime - an English sentence inside a Russian
            // screen. This is where it is caught instead.
            const auto keysOf = [] (const char* data, int size)
            {
                juce::StringArray keys;
                const auto parsed = juce::JSON::parse (juce::String::fromUTF8 (data, size));

                if (auto* object = parsed.getDynamicObject())
                    for (const auto& property : object->getProperties())
                        keys.add (property.name.toString());

                return keys;
            };

            const auto english = keysOf (BinaryData::en_json, BinaryData::en_jsonSize);
            expect (english.size() > 500);

            struct Table { const char* code; const char* data; int size; };
            const Table tables[] = {
                { "ru", BinaryData::ru_json, BinaryData::ru_jsonSize },
                { "de", BinaryData::de_json, BinaryData::de_jsonSize },
                { "fr", BinaryData::fr_json, BinaryData::fr_jsonSize },
                { "es", BinaryData::es_json, BinaryData::es_jsonSize },
                { "pt", BinaryData::pt_json, BinaryData::pt_jsonSize },
                { "zh-Hans", BinaryData::zhHans_json, BinaryData::zhHans_jsonSize },
                { "ja", BinaryData::ja_json, BinaryData::ja_jsonSize },
                { "ko", BinaryData::ko_json, BinaryData::ko_jsonSize },
                { "it", BinaryData::it_json, BinaryData::it_jsonSize },
                { "pl", BinaryData::pl_json, BinaryData::pl_jsonSize },
                { "uk", BinaryData::uk_json, BinaryData::uk_jsonSize },
            };

            for (const auto& table : tables)
            {
                const auto keys = keysOf (table.data, table.size);

                for (const auto& key : english)
                    expect (keys.contains (key), juce::String (table.code) + " is missing " + key);

                for (const auto& key : keys)
                    expect (english.contains (key), juce::String (table.code) + " has a key English does not: " + key);
            }
        }

        beginTest ("getText returns the key itself for an unknown key");
        {
            juce::PropertiesFile props (makeOptions ("unknown_key"));
            LocalisationManager manager (props);
            expectEquals (manager.getText ("this.key.does.not.exist"), juce::String ("this.key.does.not.exist"));
        }

        beginTest ("an unsupported saved language code falls back to a supported one, not a crash");
        {
            juce::PropertiesFile props (makeOptions ("fallback"));
            props.setValue ("language", "xx-not-a-real-language");
            LocalisationManager manager (props);

            expect (LocalisationManager::getSupportedLanguageCodes().contains (manager.getCurrentLanguage()));
            expect (manager.getText ("ui.close").isNotEmpty());
        }

        beginTest ("setLanguage switches the active language and its getText output");
        {
            // Doesn't assert on the ChangeListener actually firing:
            // sendChangeMessage() is asynchronous and needs a running
            // JUCE message loop to deliver, which EarTrainerTests never
            // pumps - the same reason ProgressManager exposes a direct
            // registerAnswer() entry point instead of relying on
            // ChangeListener delivery in tests. setLanguage() calling
            // sendChangeMessage() is still real, correct behaviour for
            // the actual plugin editors listening at runtime; it's just
            // not observable from this console test binary.
            juce::PropertiesFile props (makeOptions ("switch"));
            props.setValue ("language", "en");
            LocalisationManager manager (props);

            const auto englishBypass = manager.getText ("ui.close");   // not ui.bypass: that one stays English everywhere now
            manager.setLanguage ("ru");

            expectEquals (manager.getCurrentLanguage(), juce::String ("ru"));
            expect (manager.getText ("ui.close").isNotEmpty());
            expect (manager.getText ("ui.close") != englishBypass);
        }

        beginTest ("setLanguage is a no-op for an unsupported code");
        {
            juce::PropertiesFile props (makeOptions ("noop"));
            props.setValue ("language", "en");
            LocalisationManager manager (props);

            manager.setLanguage ("not-a-real-code");
            expectEquals (manager.getCurrentLanguage(), juce::String ("en"));
        }

        beginTest ("placeholder substitution replaces {{name}} with the given value");
        {
            juce::PropertiesFile props (makeOptions ("placeholders"));
            props.setValue ("language", "en");
            LocalisationManager manager (props);

            const auto text = manager.getText ("ui.level", { { "level", "5" } });
            expectEquals (text, juce::String ("Level 5"));

            const auto scoreText = manager.getText ("ui.score", { { "correct", "3" }, { "total", "10" } });
            expectEquals (scoreText, juce::String ("Score: 3 / 10"));
        }

        beginTest ("display names are non-empty for every supported language");
        {
            for (const auto& code : LocalisationManager::getSupportedLanguageCodes())
                expect (LocalisationManager::getDisplayName (code).isNotEmpty());
        }
    }
};

static LocalisationManagerTest localisationManagerTest;

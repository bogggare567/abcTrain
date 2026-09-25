#include <juce_core/juce_core.h>
#include "shared/i18n/LocalisationManager.h"

// Word forms agree with numbers: "1 раунд, 3 раунда, 5 раундов", and every
// "[[...]]" in the string tables has as many forms as its language needs.
namespace { juce::String u (const char* utf8) { return juce::String::fromUTF8 (utf8); } }

class PluralTest : public juce::UnitTest
{
public:
    PluralTest() : juce::UnitTest ("Plurals", "i18n") {}

    void runTest() override
    {
        using L = LocalisationManager;

        beginTest ("Russian: one, few, many");
        {
            const auto t = u ("{{n}} [[раунд|раунда|раундов]]");
            const auto f = [&t] (int n) { return L::resolvePlurals (t.replace ("{{n}}", juce::String (n)), "ru"); };
            expectEquals (f (1), u ("1 раунд"));
            expectEquals (f (2), u ("2 раунда"));
            expectEquals (f (4), u ("4 раунда"));
            expectEquals (f (5), u ("5 раундов"));
            expectEquals (f (11), u ("11 раундов"));
            expectEquals (f (12), u ("12 раундов"));
            expectEquals (f (21), u ("21 раунд"));
            expectEquals (f (22), u ("22 раунда"));
            expectEquals (f (0), u ("0 раундов"));
            expectEquals (f (111), u ("111 раундов"));
        }

        beginTest ("Polish and English");
        {
            expectEquals (L::resolvePlurals ("1 [[runda|rundy|rund]]", "pl"), juce::String ("1 runda"));
            expectEquals (L::resolvePlurals ("22 [[runda|rundy|rund]]", "pl"), juce::String ("22 rundy"));
            expectEquals (L::resolvePlurals ("21 [[runda|rundy|rund]]", "pl"), juce::String ("21 rund"));
            expectEquals (L::resolvePlurals ("1 [[round|rounds]]", "en"), juce::String ("1 round"));
            expectEquals (L::resolvePlurals ("0 [[round|rounds]]", "en"), juce::String ("0 rounds"));
            expectEquals (L::resolvePlurals ("0 [[manche|manches]]", "fr"), juce::String ("0 manche"));
            expectEquals (L::resolvePlurals (u ("3 [[回]]"), "ja"), u ("3 回"));
        }

        beginTest ("several numbers in one line, each with its own word");
        {
            expectEquals (L::resolvePlurals (u ("Серия: 3 [[день|дня|дней]], 1 [[раунд|раунда|раундов]]"), "ru"),
                          u ("Серия: 3 дня, 1 раунд"));
            expectEquals (L::resolvePlurals ("no brackets 5", "ru"), juce::String ("no brackets 5"));
        }

        beginTest ("every [[...]] in the tables has the forms its language needs");
        {
            const std::map<juce::String, int> needed { { "ru", 3 }, { "uk", 3 }, { "pl", 3 }, { "ja", 1 }, { "ko", 1 }, { "zh-Hans", 1 } };

            for (const auto& language : L::getSupportedLanguageCodes())
            {
                juce::MemoryOutputStream unused;
                const auto file = juce::File (__FILE__).getParentDirectory().getParentDirectory()
                                      .getChildFile ("shared/i18n/strings").getChildFile (language + ".json");

                if (! file.existsAsFile())
                    continue;

                const auto table = juce::JSON::parse (file);
                const auto want = needed.count (language) > 0 ? needed.at (language) : 2;

                if (auto* object = table.getDynamicObject())
                {
                    for (const auto& property : object->getProperties())
                    {
                        auto text = property.value.toString();

                        while (text.contains ("[["))
                        {
                            const auto forms = juce::StringArray::fromTokens (text.fromFirstOccurrenceOf ("[[", false, false)
                                                                                  .upToFirstOccurrenceOf ("]]", false, false), "|", "");
                            expect (forms.size() >= want, language + " " + property.name.toString() + ": " + juce::String (forms.size())
                                                              + " forms, needs " + juce::String (want));
                            text = text.fromFirstOccurrenceOf ("]]", false, false);
                        }
                    }
                }
            }
        }
    }
};

static PluralTest pluralTest;

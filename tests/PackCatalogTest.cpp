#include <juce_core/juce_core.h>
#include "../shared/audio/PackCatalog.h"

// s03: the list of packs comes from GitHub's releases JSON - only .zip
// assets, drafts skipped, newest release first as GitHub sends them.
class PackCatalogTest : public juce::UnitTest
{
public:
    PackCatalogTest() : juce::UnitTest ("PackCatalog", "Audio") {}

    void runTest() override
    {
        beginTest ("zip assets of published releases, nothing else");
        const auto json = R"([
          {"name":"Rock basics 1.1","tag_name":"rock-1.1","draft":false,"assets":[
            {"name":"rock-basics-1.1.0.zip","size":5242880,"browser_download_url":"https://example/rock.zip"},
            {"name":"CREDITS.md","size":900,"browser_download_url":"https://example/credits"}]},
          {"name":"Draft","draft":true,"assets":[{"name":"x.zip","size":1,"browser_download_url":"https://example/x.zip"}]},
          {"tag_name":"vocals-1.0","assets":[{"name":"vocals.ZIP","size":10,"browser_download_url":"https://example/v.zip"}]}
        ])";
        const auto list = PackCatalog::parse (json);
        expectEquals ((int) list.size(), 2);
        expectEquals (list[0].name, juce::String ("rock-basics-1.1.0.zip"));
        expectEquals (list[0].release, juce::String ("Rock basics 1.1"));
        expect (list[0].bytes == 5242880);
        expectEquals (list[1].release, juce::String ("vocals-1.0"));

        beginTest ("not an array: nothing, no crash");
        expect (PackCatalog::parse (R"({"message":"Not Found"})").empty());
        expect (PackCatalog::parse ("garbage").empty());
    }
};

static PackCatalogTest packCatalogTest;

#include <juce_core/juce_core.h>
#include "../shared/UpdateChecker.h"

// Only the pure logic (isNewerVersion/parseReleaseJson) is exercised here -
// checkForUpdatesAsync's real network call to GitHub is deliberately not
// tested: it needs a live connection, and this console test binary is
// meant to run deterministically offline. See decisions/007-update-checker.md.
class UpdateCheckerTest : public juce::UnitTest
{
public:
    UpdateCheckerTest() : juce::UnitTest ("UpdateChecker", "Plugins") {}

    void runTest() override
    {
        beginTest ("isNewerVersion recognises a newer patch/minor/major release");
        {
            expect (UpdateChecker::isNewerVersion ("v0.2.0", "v0.1.0"));
            expect (UpdateChecker::isNewerVersion ("v1.0.0", "v0.9.9"));
            expect (UpdateChecker::isNewerVersion ("v0.1.1", "v0.1.0"));
        }

        beginTest ("isNewerVersion is false for an equal or older version");
        {
            expect (! UpdateChecker::isNewerVersion ("v0.1.0", "v0.1.0"));
            expect (! UpdateChecker::isNewerVersion ("v0.1.0", "v0.2.0"));
            expect (! UpdateChecker::isNewerVersion ("v0.9.0", "v1.0.0"));
        }

        beginTest ("isNewerVersion works with or without a leading 'v', and mismatched component counts");
        {
            expect (UpdateChecker::isNewerVersion ("0.2.0", "0.1.0"));
            expect (UpdateChecker::isNewerVersion ("v0.2.0", "0.1.0"));
            expect (UpdateChecker::isNewerVersion ("0.2", "0.1.9")); // 0.2 == 0.2.0 > 0.1.9
            expect (! UpdateChecker::isNewerVersion ("0.1.0", "0.1")); // equal (0.1 == 0.1.0)
        }

        beginTest ("isNewerVersion returns false rather than guessing for malformed input");
        {
            expect (! UpdateChecker::isNewerVersion ("not-a-version", "v0.1.0"));
            expect (! UpdateChecker::isNewerVersion ("v0.2.0", "not-a-version"));
            expect (! UpdateChecker::isNewerVersion ("", "v0.1.0"));
            expect (! UpdateChecker::isNewerVersion ("v1.2.x", "v0.1.0"));
        }

        beginTest ("parseReleaseJson extracts tag_name and html_url from a well-formed response");
        {
            const auto json = R"({
                "tag_name": "v0.2.0",
                "html_url": "https://github.com/bogggare567/abcTrain/releases/tag/v0.2.0",
                "name": "v0.2.0",
                "draft": false
            })";

            const auto release = UpdateChecker::parseReleaseJson (json);

            expectEquals (release.tagName, juce::String ("v0.2.0"));
            expectEquals (release.htmlUrl, juce::String ("https://github.com/bogggare567/abcTrain/releases/tag/v0.2.0"));
        }

        beginTest ("parseReleaseJson returns an empty tagName for malformed or unexpected JSON");
        {
            expect (UpdateChecker::parseReleaseJson ("not json at all").tagName.isEmpty());
            expect (UpdateChecker::parseReleaseJson ("").tagName.isEmpty());
            expect (UpdateChecker::parseReleaseJson ("[1, 2, 3]").tagName.isEmpty()); // valid JSON, not an object
            expect (UpdateChecker::parseReleaseJson (R"({"message": "Not Found"})").tagName.isEmpty()); // GitHub's 404 shape
        }
    }
};

static UpdateCheckerTest updateCheckerTest;

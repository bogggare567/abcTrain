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

        beginTest ("a build between tags still sees a newer release");
        {
            // git describe gives this shape for any commit past a tag.
            // Rejecting it (which the first implementation did) left the
            // update check permanently answering "up to date" for anyone
            // not sitting exactly on a tag.
            expect (UpdateChecker::isNewerVersion ("v1.1.0", "v1.0.0-3-gabc1234"));
            expect (UpdateChecker::isNewerVersion ("v1.0.1", "v1.0.0-12-gdeadbee-dirty"));

            // ...and the same build is not told to "update" to the tag it
            // is already past.
            expect (! UpdateChecker::isNewerVersion ("v1.0.0", "v1.0.0-3-gabc1234"));

            // Pre-release suffixes compare on their numbers too.
            expect (UpdateChecker::isNewerVersion ("v2.0.0", "v1.9.9-beta"));
            expect (! UpdateChecker::isNewerVersion ("v1.0.0-beta", "v1.0.0"));
        }

        beginTest ("a version that is only metadata is still rejected");
        {
            // No numeric part at all - there is nothing to compare, so
            // guessing would be worse than declining.
            expect (! UpdateChecker::isNewerVersion ("v1.0.0", "dev"));
            expect (! UpdateChecker::isNewerVersion ("nightly", "v1.0.0"));
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

        beginTest ("parseReleaseListJson(allowPrerelease=true) returns the newest entry regardless of its prerelease flag");
        {
            const auto json = R"([
                { "tag_name": "v1.0.0-beta1", "html_url": "https://example.com/beta1", "prerelease": true },
                { "tag_name": "v0.9.0", "html_url": "https://example.com/stable", "prerelease": false }
            ])";

            const auto release = UpdateChecker::parseReleaseListJson (json, true);
            expectEquals (release.tagName, juce::String ("v1.0.0-beta1"));
        }

        beginTest ("parseReleaseListJson(allowPrerelease=false) skips leading pre-releases");
        {
            const auto json = R"([
                { "tag_name": "v1.0.0-beta1", "html_url": "https://example.com/beta1", "prerelease": true },
                { "tag_name": "v0.9.0", "html_url": "https://example.com/stable", "prerelease": false }
            ])";

            const auto release = UpdateChecker::parseReleaseListJson (json, false);
            expectEquals (release.tagName, juce::String ("v0.9.0"));
        }

        beginTest ("parseReleaseListJson returns an empty tagName when every entry is a pre-release and none are allowed");
        {
            const auto json = R"([
                { "tag_name": "v1.0.0-beta2", "html_url": "https://example.com/beta2", "prerelease": true },
                { "tag_name": "v1.0.0-beta1", "html_url": "https://example.com/beta1", "prerelease": true }
            ])";

            expect (UpdateChecker::parseReleaseListJson (json, false).tagName.isEmpty());
        }

        beginTest ("parseReleaseListJson returns an empty tagName for malformed or unexpected JSON");
        {
            expect (UpdateChecker::parseReleaseListJson ("not json at all", true).tagName.isEmpty());
            expect (UpdateChecker::parseReleaseListJson ("", true).tagName.isEmpty());
            expect (UpdateChecker::parseReleaseListJson (R"({"message": "Not Found"})", true).tagName.isEmpty()); // an object, not an array
            expect (UpdateChecker::parseReleaseListJson ("[]", true).tagName.isEmpty()); // valid, empty array
        }
    }
};

static UpdateCheckerTest updateCheckerTest;

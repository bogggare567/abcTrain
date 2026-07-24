#pragma once

#include <juce_core/juce_core.h>
#include <functional>

// Checks GitHub's "latest release" API for a newer tagged version than the
// one currently running, and reports back on the message thread. Version
// comparison and JSON parsing are pure functions, separated from the actual
// network call specifically so they're unit-testable without a real
// network connection - see tests/UpdateCheckerTest.cpp and
// decisions/007-update-checker.md.
//
// Deliberately manual-trigger only for this pass: no background daily
// timer. A plugin making its own unsolicited background network calls is a
// bigger step than a "Check for Updates" button the user clicks - see the
// ADR for the full reasoning behind that trim.
namespace UpdateChecker
{
    struct ReleaseInfo
    {
        juce::String tagName; // e.g. "v0.2.0" - empty if nothing usable was found
        juce::String htmlUrl; // the release's GitHub page, for "open in browser"
    };

    // Compares two "vX.Y.Z" (or "X.Y.Z") version strings component by
    // component as integers. Returns false (not newer) for anything that
    // doesn't parse as a dotted list of non-negative integers, rather than
    // guessing - a malformed tag should never be reported as an update.
    bool isNewerVersion (const juce::String& latest, const juce::String& current) noexcept;

    // Parses the subset of GitHub's "get the latest release" API response
    // (https://docs.github.com/rest/releases/releases#get-the-latest-release)
    // this project actually needs. Returns a ReleaseInfo with an empty
    // tagName if the JSON is malformed or missing the fields it needs.
    ReleaseInfo parseReleaseJson (const juce::String& json);

    // Fetches the latest release from GitHub on a background thread and
    // invokes `callback` on the message thread with (foundNewer, release).
    // Silently does nothing (never calls back) on any failure - no
    // internet, rate limiting, an unexpected response shape - so a plugin
    // running offline or in a network-sandboxed host is never bothered by
    // this.
    void checkForUpdatesAsync (const juce::String& currentVersion,
                                std::function<void (bool foundNewer, ReleaseInfo release)> callback);
}

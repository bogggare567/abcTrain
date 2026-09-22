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

        // The installer for the machine this is running on, picked out of
        // the release's assets. Empty if the release has no asset matching
        // this platform - a source-only tag, or a build still in flight.
        //
        // Telling somebody "a new version exists, go and find it" is not an
        // update mechanism, it is a notification with homework.
        juce::String assetUrl;
        juce::String assetName;
        juce::int64  assetBytes = 0;
    };

    // Which filename ending identifies this platform's installer in a
    // release's asset list: ".dmg", "-setup.exe", ".tar.gz".
    juce::String installerSuffixForThisPlatform();

    // stable checks GitHub's "latest release" endpoint, which already
    // excludes pre-releases; beta checks the full releases list and takes
    // its first (i.e. newest) entry regardless of its prerelease flag, so
    // a "vX.Y.Z-betaN" tag is visible to anyone who opted into it.
    enum class Channel { stable, beta };

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

    // Parses GitHub's "list releases" endpoint (a JSON array, newest
    // first: https://docs.github.com/rest/releases/releases#list-releases)
    // and returns the first entry - if allowPrerelease is false, the
    // first entry whose own "prerelease" field is false, skipping any
    // newer pre-releases in between. Returns a ReleaseInfo with an empty
    // tagName if the JSON is malformed, isn't an array, or (with
    // allowPrerelease false) every entry is a pre-release.
    ReleaseInfo parseReleaseListJson (const juce::String& json, bool allowPrerelease);

    // Fetches the latest release from GitHub on a background thread and
    // invokes `callback` on the message thread with (foundNewer, release).
    // Silently does nothing (never calls back) on any failure - no
    // internet, rate limiting, an unexpected response shape - so a plugin
    // running offline or in a network-sandboxed host is never bothered by
    // this. The two-argument overload is Channel::stable, unchanged from
    // before beta-channel support existed.
    void checkForUpdatesAsync (const juce::String& currentVersion, Channel channel,
                                std::function<void (bool foundNewer, ReleaseInfo release)> callback);

    // Downloads `release`'s installer into the user's Downloads folder,
    // reporting 0..1 and then the finished file (or an invalid File on
    // failure) - both on the message thread.
    //
    // It stops at "the installer is now on your disk, here it is". It does
    // not run it: installing a plugin means writing into a shared system
    // folder, with admin rights, while the host has that very plugin
    // loaded. An updater that did it silently would be a plugin that can
    // pull the floor out from under a session in progress.
    void downloadReleaseAsync (const ReleaseInfo& release,
                                std::function<void (float progress)> onProgress,
                                std::function<void (juce::File)> onFinished);

    inline void checkForUpdatesAsync (const juce::String& currentVersion,
                                       std::function<void (bool foundNewer, ReleaseInfo release)> callback)
    {
        checkForUpdatesAsync (currentVersion, Channel::stable, std::move (callback));
    }
}

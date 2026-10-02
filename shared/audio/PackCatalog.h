#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <vector>

// Sound packs from the abcTrain-library repository's GitHub Releases (s03,
// docs/design/sound-library.md). Why at all: the installer keeps only the
// starter pack, so it stays megabytes rather than hundreds; a new pack -
// a guest engineer's drums, a genre set - reaches players without a new app
// release; and everything in it passed build_pack.py's licence check.
//
// Nothing happens unless the player presses the button: one request for
// the list, one download for the pack chosen - the same rule as the update
// check (ADR 042). The zip then goes through the ordinary import
// (ReferenceAudioLibrary::installPack).
namespace PackCatalog
{
    inline constexpr const char* releasesUrl = "https://api.github.com/repos/bogggare567/abcTrain-library/releases";

    struct Entry
    {
        juce::String name;          // the asset, "rock-basics-1.0.0.zip"
        juce::String release;       // the release title
        juce::String url;           // browser_download_url
        juce::int64 bytes = 0;
    };

    // Every .zip asset of every release, newest first. Parsing is separate so
    // a test can feed it the API's JSON.
    std::vector<Entry> parse (const juce::String& json);

    // On a background thread; `done` comes back on the message thread with
    // the list, or an empty list and a reason.
    void fetch (std::function<void (std::vector<Entry>, juce::String error)> done);

    // Downloads to a temporary file; `done` on the message thread with the
    // file (empty on failure).
    void download (const Entry&, std::function<void (juce::File)> done);
}

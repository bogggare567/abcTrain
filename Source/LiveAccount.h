#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>

class ProgressManager;

// The app's side of a Live account (ADR 045; the server side is
// soundkorb.ru, docs in the site repo: docs/abctrain-live-server.md).
//
// No passwords in the app, ever. Signing in is linking this computer:
//   1. the app asks the server for a short code (link/start);
//   2. the player opens soundkorb.ru/link?code=..., signs in there with an
//      e-mail code and presses "Connect";
//   3. the app, polling, receives a device token exactly once (link/poll).
// The token is a revocable key for this computer only; the site lists the
// computers and can switch any of them off. It is kept in this user's
// application-data folder (the same place as the progress file) - not a
// secret from the person using the computer, and not worth more than the
// progress it syncs.
//
// What goes to the server is ProgressManager::makeSyncSummary - about a
// kilobyte - and only while signed in with sync on. Everything else stays
// here. The offline rule holds: nothing is sent unless the player signed in.
//
// Every request runs on one worker thread; every callback comes back on
// the message thread. Destroying this cancels what is in flight.
class LiveAccount : public juce::ChangeBroadcaster,
                    private juce::Timer
{
public:
    explicit LiveAccount (ProgressManager&);
    ~LiveAccount() override;

    // https://soundkorb.ru, or ABCTRAIN_LIVE_URL for a local test server.
    static juce::String baseUrl();

    // A separate settings file ("abcTrain/…live"): the token must never end
    // up in anything that is exported, shared or attached to a bug report.
    static juce::PropertiesFile::Options makeDefaultOptions();

    // Tests: a temp file instead of the real one.
    LiveAccount (ProgressManager&, const juce::PropertiesFile::Options&);

    bool isSignedIn() const;
    juce::String getNick() const;
    juce::String getCountry() const;

    // ---- signing in ---------------------------------------------------------
    enum class LinkStage { idle, starting, waiting, approved, expired, failed };

    struct Link
    {
        LinkStage stage = LinkStage::idle;
        juce::String code, url;
        double expiresAtMs = 0.0;
        juce::String error;      // a short reason in English, for logs; the UI has its own words
    };

    // Asks for a code and starts polling. `openBrowser`: open the url as
    // soon as it is known (the button the player pressed asked for that).
    void startSignIn (bool openBrowser);
    void cancelSignIn();
    const Link& getLink() const noexcept { return link; }

    // tools/EditorSnapshots: a code on screen without asking any server.
    void showLinkForSnapshot (const juce::String& code)
    {
        link.stage = LinkStage::waiting;
        link.code = code;
        link.expiresAtMs = juce::Time::getMillisecondCounterHiRes() + 8.0 * 60.0 * 1000.0;
    }

    void signOut();

    // ---- sync ---------------------------------------------------------------
    void setSyncEnabled (bool);
    bool isSyncEnabled() const;
    void syncNow();
    juce::Time getLastSyncTime() const;
    bool isBusy() const noexcept { return busy.load() > 0; }

    enum class SyncResult { none, ok, offline, signedOut, error };
    SyncResult getLastSyncResult() const noexcept { return lastSync; }

    // ---- rating (public, no account needed) ---------------------------------
    struct RatingRow
    {
        int place = 0;
        juce::String nick, country;
        int rating = 0, deviation = 0, wins = 0, losses = 0, today = 0;
        bool provisional = false;
    };

    // family: freq / dyn / space / char; country empty = world.
    void fetchRating (const juce::String& family, const juce::String& country,
                      std::function<void (bool ok, juce::Array<RatingRow>)> done);

    // Anything visible changed - signed in or out, a link stage, a sync -
    // is a change message (several screens listen), plus this hook.
    std::function<void()> onChanged;

    // ---- the pure parts, for tests -----------------------------------------
    static juce::String deviceName();
    static juce::String appVersion (const juce::String& gitDescribe);   // "v2.0.0-3-gabc" -> "2.0.0-3-gabc"
    static juce::String platform();
    static juce::Array<RatingRow> parseRating (const juce::String& json);

private:
    void timerCallback() override;
    void pollOnce();
    void refreshMe();
    void performSync();
    void notify();

    struct Response
    {
        int status = 0;
        juce::var json;
        bool reached() const noexcept { return status != 0; }
    };

    // Blocking; worker thread only.
    static Response request (const juce::String& method, const juce::String& path,
                             const juce::var& body, const juce::String& bearer);

    void run (std::function<void()> job);

    ProgressManager& progress;
    std::unique_ptr<juce::PropertiesFile> properties;
    juce::ThreadPool pool { 1 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::atomic<int> busy { 0 };

    Link link;
    juce::String pollToken;
    SyncResult lastSync = SyncResult::none;
    juce::uint64 syncedChangeCounter = 0;
    int syncTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveAccount)
};

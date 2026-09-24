#include "LiveAccount.h"
#include "ProgressManager.h"
#include "LiveLink.h"
#include "shared/updates/Version.h"

namespace
{
    constexpr const char* tokenKey = "deviceToken";
    constexpr const char* nickKey = "nick";
    constexpr const char* countryKey = "country";
    constexpr const char* syncEnabledKey = "syncEnabled";
    constexpr const char* syncVersionKey = "syncVersion";
    constexpr const char* lastSyncKey = "lastSyncMs";

    constexpr int pollIntervalMs = 2500;
    constexpr int syncEveryTicks = (15 * 60 * 1000) / pollIntervalMs;   // a quarter of an hour
}

LiveAccount::LiveAccount (ProgressManager& progressToSync)
    : LiveAccount (progressToSync, makeDefaultOptions())
{
}

LiveAccount::LiveAccount (ProgressManager& progressToSync, const juce::PropertiesFile::Options& options)
    : progress (progressToSync),
      properties (std::make_unique<juce::PropertiesFile> (options))
{
    syncedChangeCounter = progress.getChangeCounter();

    // Signed in with sync on: one sync shortly after launch (not during
    // it - the window comes first), then every quarter of an hour when
    // something changed. Signed out: this timer never starts.
    if (isSignedIn() && isSyncEnabled() && LiveLink::networkAllowed.load())
    {
        juce::Timer::callAfterDelay (5000, [flag = alive, this]
        {
            if (flag->load())
                syncNow();
        });
        startTimer (pollIntervalMs);
    }
}

LiveAccount::~LiveAccount()
{
    alive->store (false);
    stopTimer();
    pool.removeAllJobs (true, 10000);
}

juce::PropertiesFile::Options LiveAccount::makeDefaultOptions()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "abcTrain";
    options.filenameSuffix = "live";
    options.folderName = "abcTrain";
    options.osxLibrarySubFolder = "Application Support";
    return options;
}

juce::String LiveAccount::baseUrl()
{
    const auto fromEnv = juce::SystemStats::getEnvironmentVariable ("ABCTRAIN_LIVE_URL", {});
    return fromEnv.isNotEmpty() ? fromEnv.trimCharactersAtEnd ("/") : juce::String ("https://soundkorb.ru");
}

juce::String LiveAccount::deviceName()
{
    auto name = juce::SystemStats::getComputerName().trim();

    if (name.isEmpty())
        name = juce::SystemStats::getOperatingSystemName();

    return name.substring (0, 60);
}

juce::String LiveAccount::appVersion (const juce::String& describe)
{
    // "v2.0.0" and "v1.8.0-beta.1-8-g2178654-dirty" come from git describe;
    // the server takes "2.0.0" with a short suffix. The leading "v" made
    // every link request fail with 400 - LiveSmoke caught it.
    auto version = describe.trim();
    if (version.startsWithIgnoreCase ("v"))
        version = version.substring (1);

    const auto core = version.initialSectionContainingOnly ("0123456789.");
    if (juce::StringArray::fromTokens (core, ".", "").size() != 3 || core.endsWith ("."))
        return "0.0.0";

    // One separator ("-" or "+"), then up to 20 of [0-9A-Za-z.-].
    auto rest = version.substring (core.length());
    const auto separator = rest.startsWith ("+") ? juce::String ("+") : juce::String ("-");
    rest = rest.trimCharactersAtStart ("-+").replaceCharacter ('+', '.')
               .retainCharacters ("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-")
               .substring (0, 20);

    return rest.isEmpty() ? core : core + separator + rest;
}

juce::String LiveAccount::platform()
{
   #if JUCE_MAC
    return "macOS";
   #elif JUCE_WINDOWS
    return "Windows";
   #elif JUCE_LINUX
    return "Linux";
   #else
    return "other";
   #endif
}

bool LiveAccount::isSignedIn() const       { return properties->getValue (tokenKey).isNotEmpty(); }
juce::String LiveAccount::getNick() const  { return properties->getValue (nickKey); }
juce::String LiveAccount::getCountry() const { return properties->getValue (countryKey); }
bool LiveAccount::isSyncEnabled() const    { return properties->getBoolValue (syncEnabledKey, true); }

juce::Time LiveAccount::getLastSyncTime() const
{
    return juce::Time ((juce::int64) properties->getDoubleValue (lastSyncKey, 0.0));
}

void LiveAccount::notify()
{
    sendChangeMessage();

    if (onChanged != nullptr)
        onChanged();
}

void LiveAccount::run (std::function<void()> job)
{
    ++busy;
    pool.addJob ([this, flag = alive, job = std::move (job)]
    {
        if (flag->load())
            job();

        --busy;
    });
}

LiveAccount::Response LiveAccount::request (const juce::String& method, const juce::String& path,
                                            const juce::var& body, const juce::String& bearer)
{
    Response response;
    auto url = juce::URL (baseUrl() + path);

    juce::String headers;
    headers << "Accept: application/json\r\n"
            << "User-Agent: abcTrain/" << CurrentVersion::string << "\r\n";

    if (bearer.isNotEmpty())
        headers << "Authorization: Bearer " << bearer << "\r\n";

    const auto hasBody = ! body.isVoid();

    if (hasBody)
    {
        headers << "Content-Type: application/json\r\n";
        url = url.withPOSTData (juce::JSON::toString (body, true));
    }

    auto options = juce::URL::InputStreamOptions (hasBody ? juce::URL::ParameterHandling::inPostData
                                                          : juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders (headers)
                       .withConnectionTimeoutMs (8000)
                       .withNumRedirectsToFollow (0)
                       .withStatusCode (&response.status)
                       .withHttpRequestCmd (method);

    auto stream = url.createInputStream (options);

    if (stream == nullptr)
    {
        response.status = 0;
        return response;
    }

    // The server's answers are small; anything larger is not an answer.
    juce::MemoryOutputStream text;
    text.writeFromInputStream (*stream, 256 * 1024);
    response.json = juce::JSON::parse (text.toString());
    return response;
}

// ---- signing in ----------------------------------------------------------------

void LiveAccount::startSignIn (bool openBrowser)
{
    if (link.stage == LinkStage::starting || link.stage == LinkStage::waiting)
    {
        if (openBrowser && link.url.isNotEmpty())
            juce::URL (link.url).launchInDefaultBrowser();
        return;
    }

    link = {};
    link.stage = LinkStage::starting;
    notify();

    auto* object = new juce::DynamicObject();
    object->setProperty ("deviceName", deviceName());
    object->setProperty ("platform", platform());
    object->setProperty ("appVersion", appVersion (CurrentVersion::string));
    const juce::var body (object);

    run ([this, body, openBrowser, flag = alive]
    {
        const auto response = request ("POST", "/api/abctrain/link/start", body, {});

        juce::MessageManager::callAsync ([this, flag, response, openBrowser]
        {
            if (! flag->load() || link.stage != LinkStage::starting)
                return;

            if (response.status != 200 || ! (bool) response.json.getProperty ("ok", false))
            {
                link.stage = LinkStage::failed;
                link.error = response.reached() ? "status " + juce::String (response.status) : "no connection";
                notify();
                return;
            }

            link.stage = LinkStage::waiting;
            link.code = response.json.getProperty ("code", "").toString();
            link.url = response.json.getProperty ("url", "").toString();
            link.expiresAtMs = juce::Time::getMillisecondCounterHiRes()
                             + 1000.0 * (double) (int) response.json.getProperty ("expiresIn", 600);
            pollToken = response.json.getProperty ("pollToken", "").toString();

            // Only a link to our own site is ever opened, whatever a
            // response says.
            if (openBrowser && link.url.startsWith (baseUrl() + "/"))
                juce::URL (link.url).launchInDefaultBrowser();

            startTimer (pollIntervalMs);
            notify();
        });
    });
}

void LiveAccount::cancelSignIn()
{
    link = {};
    pollToken.clear();
    notify();
}

void LiveAccount::timerCallback()
{
    if (link.stage == LinkStage::waiting)
    {
        if (juce::Time::getMillisecondCounterHiRes() > link.expiresAtMs)
        {
            link.stage = LinkStage::expired;
            pollToken.clear();
            notify();
        }
        else if (busy.load() == 0)
        {
            pollOnce();
        }

        return;
    }

    if (! isSignedIn() || ! isSyncEnabled())
    {
        stopTimer();
        return;
    }

    if (++syncTicks >= syncEveryTicks)
    {
        syncTicks = 0;

        if (progress.getChangeCounter() != syncedChangeCounter)
            syncNow();
    }
}

void LiveAccount::pollOnce()
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("pollToken", pollToken);
    const juce::var body (object);

    run ([this, body, flag = alive]
    {
        const auto response = request ("POST", "/api/abctrain/link/poll", body, {});

        juce::MessageManager::callAsync ([this, flag, response]
        {
            if (! flag->load() || link.stage != LinkStage::waiting)
                return;

            if (response.status == 429 || ! response.reached())
                return;   // too quick, or a blip: the next tick asks again

            const auto status = response.json.getProperty ("status", "").toString();

            if (status == "expired")
            {
                link.stage = LinkStage::expired;
                notify();
                return;
            }

            const auto token = response.json.getProperty ("deviceToken", "").toString();

            if (status != "approved" || token.isEmpty())
                return;

            properties->setValue (tokenKey, token);

            const auto account = response.json.getProperty ("account", {});
            properties->setValue (nickKey, account.getProperty ("nick", "").toString());
            properties->setValue (countryKey, account.getProperty ("country", "").toString());
            properties->saveIfNeeded();

            link.stage = LinkStage::approved;
            pollToken.clear();
            notify();

            if (isSyncEnabled())
                syncNow();
            else
                stopTimer();
        });
    });
}

void LiveAccount::refreshMe()
{
    const auto token = properties->getValue (tokenKey);

    run ([this, token, flag = alive]
    {
        const auto response = request ("GET", "/api/abctrain/me", {}, token);

        juce::MessageManager::callAsync ([this, flag, response]
        {
            if (! flag->load())
                return;

            if (response.status == 401)
            {
                // Switched off on the site: this computer is signed out.
                for (auto* key : { tokenKey, nickKey, countryKey, syncVersionKey })
                    properties->removeValue (key);
                properties->saveIfNeeded();
                notify();
                return;
            }

            if (response.status == 200)
            {
                const auto account = response.json.getProperty ("account", {});
                properties->setValue (nickKey, account.getProperty ("nick", "").toString());
                properties->setValue (countryKey, account.getProperty ("country", "").toString());
                properties->saveIfNeeded();
                notify();
            }
        });
    });
}

void LiveAccount::signOut()
{
    const auto token = properties->getValue (tokenKey);

    // Forget it here first: signing out must work offline too. The server
    // is told if it can be reached; if not, the site can switch the
    // computer off later.
    for (auto* key : { tokenKey, nickKey, countryKey, syncVersionKey, lastSyncKey })
        properties->removeValue (key);
    properties->saveIfNeeded();

    link = {};
    stopTimer();
    notify();

    if (token.isNotEmpty())
        run ([token] { request ("POST", "/api/abctrain/device/signout", juce::var (new juce::DynamicObject()), token); });
}

// ---- sync ------------------------------------------------------------------------

void LiveAccount::setSyncEnabled (bool shouldSync)
{
    properties->setValue (syncEnabledKey, shouldSync);
    properties->saveIfNeeded();

    if (shouldSync && isSignedIn())
    {
        startTimer (pollIntervalMs);
        syncNow();
    }

    notify();
}

void LiveAccount::syncNow()
{
    if (! isSignedIn())
    {
        lastSync = SyncResult::signedOut;
        return;
    }

    performSync();
}

void LiveAccount::performSync()
{
    const auto token = properties->getValue (tokenKey);
    const auto summary = progress.makeSyncSummary();
    const auto counter = progress.getChangeCounter();

    // Worker: fetch what the server has, send our summary on top of it.
    // Merging happens on the message thread (ProgressManager lives there),
    // so the worker only carries data back and forth.
    run ([this, token, summary, counter, flag = alive]
    {
        const auto remote = request ("GET", "/api/abctrain/sync", {}, token);

        juce::MessageManager::callAsync ([this, flag, remote, token, counter, summary]
        {
            if (! flag->load())
                return;

            if (remote.status == 401)
            {
                lastSync = SyncResult::signedOut;
                refreshMe();   // clears the token and tells the screen
                return;
            }

            if (remote.status != 200)
            {
                lastSync = remote.reached() ? SyncResult::error : SyncResult::offline;
                notify();
                return;
            }

            // Theirs into ours first, then ours (now the union) up.
            const auto data = remote.json.getProperty ("data", {});
            if (data.isObject())
                progress.mergeSyncSummary (data);

            auto* object = new juce::DynamicObject();
            object->setProperty ("baseVersion", (int) remote.json.getProperty ("version", 0));
            object->setProperty ("data", progress.makeSyncSummary());
            const juce::var body (object);
            const auto newCounter = progress.getChangeCounter();
            juce::ignoreUnused (counter, summary);

            run ([this, token, body, newCounter, flag]
            {
                const auto put = request ("PUT", "/api/abctrain/sync", body, token);

                juce::MessageManager::callAsync ([this, flag, put, newCounter]
                {
                    if (! flag->load())
                        return;

                    if (put.status == 409)
                    {
                        // Another computer synced in between: merge again
                        // next round rather than loop here.
                        lastSync = SyncResult::error;
                        juce::Timer::callAfterDelay (1500, [this, flag] { if (flag->load()) performSync(); });
                        return;
                    }

                    if (put.status != 200)
                    {
                        lastSync = put.reached() ? SyncResult::error : SyncResult::offline;
                        notify();
                        return;
                    }

                    properties->setValue (syncVersionKey, (int) put.json.getProperty ("version", 0));
                    properties->setValue (lastSyncKey, (double) juce::Time::currentTimeMillis());
                    properties->saveIfNeeded();
                    syncedChangeCounter = newCounter;
                    lastSync = SyncResult::ok;
                    notify();
                });
            });
        });
    });
}

// ---- rating ---------------------------------------------------------------------

juce::Array<LiveAccount::RatingRow> LiveAccount::parseRating (const juce::String& text)
{
    juce::Array<RatingRow> rows;
    const auto json = juce::JSON::parse (text);

    if (const auto* list = json.getProperty ("rows", {}).getArray())
        for (const auto& r : *list)
        {
            RatingRow row;
            row.place = (int) r.getProperty ("place", 0);
            row.nick = r.getProperty ("nick", "").toString().substring (0, 24);
            row.country = r.getProperty ("country", "").toString().substring (0, 2);
            row.rating = (int) r.getProperty ("rating", 0);
            row.deviation = (int) r.getProperty ("deviation", 0);
            row.wins = (int) r.getProperty ("wins", 0);
            row.losses = (int) r.getProperty ("losses", 0);
            row.today = (int) std::lround ((double) r.getProperty ("today", 0.0));
            row.provisional = (bool) r.getProperty ("provisional", false);
            rows.add (row);
        }

    return rows;
}

void LiveAccount::fetchRating (const juce::String& family, const juce::String& country,
                               std::function<void (bool, juce::Array<RatingRow>)> done)
{
    auto path = "/api/abctrain/rating?limit=50&family=" + juce::URL::addEscapeChars (family, true);

    if (country.isNotEmpty())
        path << "&country=" << juce::URL::addEscapeChars (country, true);

    run ([path, done, flag = alive]
    {
        const auto response = request ("GET", path, {}, {});
        const auto ok = response.status == 200 && (bool) response.json.getProperty ("ok", false);
        const auto rows = ok ? parseRating (juce::JSON::toString (response.json)) : juce::Array<RatingRow>();

        juce::MessageManager::callAsync ([flag, done, ok, rows]
        {
            if (flag->load() && done != nullptr)
                done (ok, rows);
        });
    });
}

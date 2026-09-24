// LiveSmoke - the Live account round trip, app side against a real server.
//
//   node server/index.js            (site repo, with ABCTRAIN_TEST_HOOKS=1,
//                                    no NODE_ENV=production, no SMTP)
//   ./LiveSmoke http://127.0.0.1:<port>
//
// Plays two computers of one player:
//   1. computer A asks for a link code (LiveAccount::startSignIn);
//   2. the "browser" signs in with an e-mail code (read from the server's
//      test hook) and confirms the code, as the /link page does;
//   3. A receives its device token, plays nine right answers (level 4),
//      syncs;
//   4. computer B links the same way and, syncing, starts at level 4;
//   5. A signs out; its token no longer works; the rating still loads.
//
// Not part of CI - it needs the site's server. The unit tests cover the
// parts that need no network (tests/LiveLinkTest, ProgressManager merge).

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <iostream>

#include "../Source/GameManager.h"
#include "../Source/ProgressManager.h"
#include "../Source/LiveAccount.h"
#include "../Source/LiveLink.h"

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n";
        if (! ok)
            ++failures;
    }

    // Runs the message loop until `done` or the timeout.
    bool waitFor (std::function<bool()> done, int timeoutMs)
    {
        const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;

        while (! done())
        {
            if (juce::Time::getMillisecondCounter() > end)
                return false;

            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        }

        return true;
    }

    // The browser's side: plain HTTP with a cookie jar of one.
    struct Browser
    {
        juce::String base, cookie;

        juce::var call (const juce::String& method, const juce::String& path, const juce::var& body, int* statusOut = nullptr)
        {
            auto url = juce::URL (base + path);
            juce::String headers = "Accept: application/json\r\nOrigin: " + base + "\r\n";

            if (cookie.isNotEmpty())
                headers << "Cookie: " << cookie << "\r\n";

            if (! body.isVoid())
            {
                headers << "Content-Type: application/json\r\n";
                url = url.withPOSTData (juce::JSON::toString (body, true));
            }

            int status = 0;
            juce::StringPairArray responseHeaders;
            auto stream = url.createInputStream (juce::URL::InputStreamOptions (body.isVoid() ? juce::URL::ParameterHandling::inAddress
                                                                                             : juce::URL::ParameterHandling::inPostData)
                                                     .withExtraHeaders (headers)
                                                     .withHttpRequestCmd (method)
                                                     .withStatusCode (&status)
                                                     .withResponseHeaders (&responseHeaders)
                                                     .withConnectionTimeoutMs (8000));
            if (statusOut != nullptr)
                *statusOut = status;

            if (stream == nullptr)
                return {};

            for (const auto& key : responseHeaders.getAllKeys())
                if (key.equalsIgnoreCase ("set-cookie"))
                    for (const auto& line : juce::StringArray::fromLines (responseHeaders[key]))
                        if (line.startsWith ("abc_session="))
                            cookie = line.upToFirstOccurrenceOf (";", false, false);

            return juce::JSON::parse (stream->readEntireStreamAsString());
        }

        static juce::var object (std::initializer_list<std::pair<const char*, juce::var>> fields)
        {
            auto* o = new juce::DynamicObject();
            for (const auto& [k, v] : fields)
                o->setProperty (k, v);
            return juce::var (o);
        }

        bool signIn (const juce::String& email)
        {
            call ("POST", "/api/abctrain/auth/start", object ({ { "email", email } }));
            const auto code = call ("GET", "/api/abctrain/__test/last-code?email=" + juce::URL::addEscapeChars (email, true), {})
                                  .getProperty ("code", "").toString();
            int status = 0;
            call ("POST", "/api/abctrain/auth/verify", object ({ { "email", email }, { "code", code } }), &status);
            return status == 200 && cookie.isNotEmpty();
        }

        bool confirm (const juce::String& linkCode)
        {
            int status = 0;
            call ("POST", "/api/abctrain/link/confirm", object ({ { "code", linkCode } }), &status);
            return status == 200;
        }
    };

    juce::PropertiesFile::Options tempOptions (const juce::String& name)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "LiveSmoke_" + name;
        o.filenameSuffix = "settings";
        o.folderName = "LiveSmoke";
        o.getDefaultFile().deleteFile();
        return o;
    }

    struct Computer
    {
        explicit Computer (const juce::String& name)
            : progress (games, tempOptions (name + "_progress")),
              account (progress, tempOptions (name + "_live"))
        {
        }

        GameManager games;
        ProgressManager progress;
        LiveAccount account;
    };

    bool link (Computer& c, Browser& browser)
    {
        c.account.startSignIn (false);

        if (! waitFor ([&] { return c.account.getLink().stage == LiveAccount::LinkStage::waiting
                                 || c.account.getLink().stage == LiveAccount::LinkStage::failed; }, 10000))
            return false;

        if (c.account.getLink().stage != LiveAccount::LinkStage::waiting)
            return false;

        if (! browser.confirm (c.account.getLink().code))
            return false;

        return waitFor ([&] { return c.account.isSignedIn(); }, 15000);
    }
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "usage: LiveSmoke http://127.0.0.1:<port>\n";
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juce;
    const juce::String base (argv[1]);

   #if JUCE_WINDOWS
    _putenv_s ("ABCTRAIN_LIVE_URL", argv[1]);
   #else
    setenv ("ABCTRAIN_LIVE_URL", argv[1], 1);
   #endif

    check (LiveAccount::baseUrl() == base.trimCharactersAtEnd ("/"), "the app talks to " + base);

    Browser browser { base.trimCharactersAtEnd ("/"), {} };
    const auto email = "smoke-" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)) + "@example.com";
    check (browser.signIn (email), "the browser signs in with an e-mail code");

    std::cout << "computer A\n";
    Computer a ("A");
    check (link (a, browser), "A links through the code and gets its key");

    for (int i = 0; i < 9; ++i)
        a.progress.registerAnswer (0, true);

    check (a.progress.getLevelForGame (0) == 4, "A: nine right answers - level 4 (" + juce::String (a.progress.getLevelForGame (0)) + ")");

    a.account.syncNow();
    check (waitFor ([&] { return a.account.getLastSyncResult() == LiveAccount::SyncResult::ok; }, 15000), "A syncs its summary");

    std::cout << "computer B\n";
    Computer b ("B");
    check (b.progress.getLevelForGame (0) == 1, "B starts fresh at level 1");
    check (link (b, browser), "B links to the same account");
    check (waitFor ([&] { return b.account.getLastSyncResult() == LiveAccount::SyncResult::ok; }, 15000), "B syncs after linking");
    check (b.progress.getLevelForGame (0) == 4, "B now starts where A is: level " + juce::String (b.progress.getLevelForGame (0)));
    check (b.progress.getBestLevelForGame (0) == 4, "B has A's record");

    std::cout << "signing out\n";
    a.account.signOut();
    check (! a.account.isSignedIn(), "A forgets its key at once");
    waitFor ([&] { return ! a.account.isBusy(); }, 8000);
    a.account.syncNow();
    check (a.account.getLastSyncResult() == LiveAccount::SyncResult::signedOut, "A cannot sync once signed out");

    bool ratingDone = false, ratingOk = false;
    b.account.fetchRating ("freq", {}, [&] (bool ok, juce::Array<LiveAccount::RatingRow>) { ratingDone = true; ratingOk = ok; });
    waitFor ([&] { return ratingDone; }, 8000);
    check (ratingOk, "the rating loads");

    b.account.signOut();
    waitFor ([&] { return ! b.account.isBusy(); }, 8000);

    std::cout << (failures == 0 ? "\nLive works end to end.\n" : "\nFAILED: " + juce::String (failures).toStdString() + "\n");
    return failures == 0 ? 0 : 1;
}

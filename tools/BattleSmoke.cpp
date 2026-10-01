// BattleSmoke - a battle between two apps, app side against a real server
// (ADR 048).
//
//   node server/index.js            (site repo, ABCTRAIN_TEST_HOOKS=1, no SMTP)
//   ./BattleSmoke http://127.0.0.1:<port>
//
// Two computers, two accounts ("smokeA", "smokeB"). Both stand in the queue
// through OnlineBattle + LiveAccount::battleCall - the code the app runs -
// and play the ten rounds the server sets: each builds the round from the
// seed exactly as the editor does (seeded GameManager game, the level from
// the server), A answers right, B answers wrong. Checked: the server never
// finds the two rounds different (no voided round), B is knocked out on HP
// before the tenth round (ADR 049), Decibelo moves by the same amount up
// and down.
//
// Not part of CI - it needs the site's server.

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <iostream>

#include "../Source/GameManager.h"
#include "../Source/ProgressManager.h"
#include "../Source/LiveAccount.h"
#include "../Source/LiveLink.h"
#include "../Source/OnlineBattle.h"

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
        o.applicationName = "BattleSmoke_" + name;
        o.filenameSuffix = "settings";
        o.folderName = "BattleSmoke";
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
        OnlineBattle battle { [this] (const juce::String& m, const juce::String& p, const juce::var& b, OnlineBattle::Done d)
                              { account.battleCall (m, p, b, std::move (d)); } };
        int answeredRound = -1;
        juce::StringArray fingerprints;   // what each round asked, to compare A with B
    };

    // What the editor does when a round starts and the player answers.
    void playRound (Computer& c, bool answerRight)
    {
        const auto& st = c.battle.getState();
        if (st.stage != OnlineBattle::Stage::playing || st.round.startsInMs > 0 || st.round.n <= c.answeredRound)
            return;

        c.answeredRound = st.round.n;
        auto& game = c.games.getGame (st.game);
        game.setSeededRounds (true);
        game.setDifficulty (st.round.level);
        game.seedNextRound (st.round.seed);
        game.newRound();

        juce::String fp;
        fp << st.round.n << ":" << game.getCorrectChoiceIndex() << ":" << juce::String (game.getCorrectNormalised(), 5);
        c.fingerprints.add (fp);

        auto* f = new juce::DynamicObject();
        f->setProperty ("continuous", game.usesContinuousScale());

        if (game.usesContinuousScale())
        {
            const auto right = game.getCorrectNormalised();
            game.submitNormalisedAnswer (answerRight ? right : (right > 0.5f ? 0.0f : 1.0f));
            f->setProperty ("chosenNorm", (double) game.getChosenNormalised());
            f->setProperty ("correctNorm", (double) game.getCorrectNormalised());
            f->setProperty ("tolerance", (double) game.getToleranceNormalised());
        }
        else
        {
            const auto right = game.getCorrectChoiceIndex();
            game.submitAnswer (answerRight ? right : (right + 1) % game.getNumChoices());
        }

        f->setProperty ("chosen", game.getChosenChoiceIndex());
        f->setProperty ("correct", game.getCorrectChoiceIndex());
        c.battle.answer (juce::var (f));
    }

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
        std::cout << "usage: BattleSmoke http://127.0.0.1:<port>\n";
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juce;
    const auto base = juce::String (argv[1]).trimCharactersAtEnd ("/");

   #if JUCE_WINDOWS
    _putenv_s ("ABCTRAIN_LIVE_URL", argv[1]);
   #else
    setenv ("ABCTRAIN_LIVE_URL", argv[1], 1);
   #endif

    const auto tag = juce::String (juce::Random::getSystemRandom().nextInt (100000));
    Browser browserA { base, {} }, browserB { base, {} };
    check (browserA.signIn ("battle-a-" + tag + "@example.com") && browserB.signIn ("battle-b-" + tag + "@example.com"), "two players sign in");

    int status = 0;
    browserA.call ("PATCH", "/api/abctrain/me", Browser::object ({ { "nick", "smokeA" + tag } }), &status);
    browserB.call ("PATCH", "/api/abctrain/me", Browser::object ({ { "nick", "smokeB" + tag } }));
    check (status == 200, "nicks set");

    Computer a ("A"), b ("B");
    check (link (a, browserA) && link (b, browserB), "both apps linked");

    for (const auto& family : { juce::String ("freq"), juce::String ("space") })
    {
        std::cout << "battle: " << family << "\n";
        a.answeredRound = b.answeredRound = -1;
        a.fingerprints.clear();
        b.fingerprints.clear();

        a.battle.search (family);
        waitFor ([&] { return a.battle.getState().stage == OnlineBattle::Stage::searching && a.battle.getState().errorCode.isEmpty(); }, 5000);
        b.battle.search (family);

        // Like CS: two searching opens an accept window; both accept.
        check (waitFor ([&] { return a.battle.getState().stage == OnlineBattle::Stage::ready
                                  && b.battle.getState().stage == OnlineBattle::Stage::ready; }, 10000),
               "a room found, waiting for Accept (" + juce::String (a.battle.getState().members) + " in it)");
        a.battle.accept (true);
        b.battle.accept (true);

        const auto matched = waitFor ([&] { return a.battle.getState().stage == OnlineBattle::Stage::playing
                                                && b.battle.getState().stage == OnlineBattle::Stage::playing; }, 20000);
        check (matched, "matched: A sees " + a.battle.getState().opponentNick + ", B sees " + b.battle.getState().opponentNick
                            + ", exercise " + juce::String (a.battle.getState().game));

        const auto finished = waitFor ([&]
        {
            playRound (a, true);
            playRound (b, false);
            return a.battle.getState().stage == OnlineBattle::Stage::finished
                && b.battle.getState().stage == OnlineBattle::Stage::finished;
        }, 90000);

        const auto& sa = a.battle.getState();
        const auto& sb = b.battle.getState();
        check (finished, "the battle finished on both sides");
        check (a.fingerprints == b.fingerprints, "every round the same on both computers (" + a.fingerprints.joinIntoString (" ") + ")");

        int voided = 0;
        for (const auto& h : sa.history)
            voided += h.voided ? 1 : 0;
        check (sa.place == 1 && sb.place == 2, "places: A first, B second");
        check (voided == 0, "no round voided by the server");
        check (sa.hpThem <= 0.0f && sa.hpYou >= 99.9f && sb.hpYou <= 0.0f && sa.outcome == "won" && sb.outcome == "lost",
               "knock-out: HP " + juce::String (sa.hpYou) + " : " + juce::String (sa.hpThem) + " after "
                   + juce::String ((int) sa.history.size()) + " rounds (" + juce::String (sa.scoreYou) + ":" + juce::String (sa.scoreThem) + ")");
        check ((int) sa.history.size() < 10, "over before the tenth round");
        check (sa.recorded && sa.delta > 0 && std::abs (sa.delta + sb.delta) < 0.01,
               "Decibelo +" + juce::String (sa.delta) + " / " + juce::String (sb.delta) + " -> " + juce::String (sa.rating));

        a.battle.reset();
        b.battle.reset();
    }

    std::cout << (failures == 0 ? "\nall ok\n" : "\nFAILED\n");
    return failures == 0 ? 0 : 1;
}

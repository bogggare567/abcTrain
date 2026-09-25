#include <juce_core/juce_core.h>
#include "../Source/OnlineBattle.h"

// The client of the battle server (ADR 048), against answers shaped exactly
// like server/lib/abctrainBattles.js sends them (scripts/abctrain-battles-test.mjs
// checks the server side). No network: the transport is a fake.
class OnlineBattleTest : public juce::UnitTest
{
public:
    OnlineBattleTest() : juce::UnitTest ("OnlineBattle", "Live") {}

    static juce::var json (const char* text) { return juce::JSON::parse (juce::String::fromUTF8 (text)); }

    void runTest() override
    {
        beginTest ("a round from the server: seed, level, times, who answered");
        {
            const auto s = OnlineBattle::parse (json (R"({"ok":true,"status":"playing","matchId":"m1","family":"freq","game":8,"rounds":7,
                "you":{"nick":"alpha","rating":1500},"opponent":{"nick":"bravo","country":"RU","rating":1484},
                "score":[2,1],"history":[{"n":0,"you":true,"them":false,"voided":false},{"n":1,"you":true,"them":true,"voided":false},{"n":2,"you":false,"them":false,"voided":true}],
                "round":{"n":3,"seed":2147483000,"level":6,"startsInMs":0,"deadlineInMs":41000,"answered":false,"opponentAnswered":true}})"), {});

            expect (s.stage == OnlineBattle::Stage::playing);
            expectEquals (s.game, 8);
            expectEquals (s.opponentNick, juce::String ("bravo"));
            expectEquals (s.opponentRating, 1484);
            expectEquals (s.scoreYou, 2);
            expectEquals ((int) s.history.size(), 3);
            expect (s.history[2].voided);
            expectEquals (s.round.n, 3);
            expect (s.round.seed == 2147483000LL, "a seed near 2^31 survives the JSON");
            expectEquals (s.round.level, 6);
            expect (s.round.opponentAnswered && ! s.round.answered);
        }

        beginTest ("the result: Decibelo delta, forfeit");
        {
            const auto s = OnlineBattle::parse (json (R"({"ok":true,"status":"finished","matchId":"m1","score":[7,0],"history":[],
                "outcome":"won","forfeit":"them","delta":16,"rating":1516,"recorded":true})"), {});
            expect (s.stage == OnlineBattle::Stage::finished);
            expectEquals (s.forfeit, juce::String ("them"));
            expectEquals (s.rating, 1516);
            expect (s.recorded);

            const auto n = OnlineBattle::parse (json (R"({"ok":true,"status":"finished","forfeit":null,"outcome":"draw"})"), {});
            expect (n.forfeit.isEmpty(), "null forfeit is no forfeit");
        }

        beginTest ("an error keeps what was known and says why");
        {
            OnlineBattle::State before;
            before.stage = OnlineBattle::Stage::searching;
            before.family = "dyn";
            const auto s = OnlineBattle::parse (json (R"({"ok":false,"error":"nick_required","description":"..."})"), before);
            expectEquals (s.errorCode, juce::String ("nick_required"));
            expectEquals (s.family, juce::String ("dyn"));
        }

        beginTest ("search, then an answer carries the match and the round");
        {
            juce::StringArray calls;
            juce::var lastBody;
            OnlineBattle battle ([&] (const juce::String& method, const juce::String& path, const juce::var& body, OnlineBattle::Done done)
            {
                calls.add (method + " " + path);
                lastBody = body;

                if (path.endsWith ("/queue"))
                    done (200, json (R"({"ok":true,"status":"playing","matchId":"m9","game":1,"round":{"n":0,"seed":5,"level":4,"startsInMs":2000}})"));
                else if (path.endsWith ("/answer"))
                    done (409, json (R"({"ok":false,"error":"wrong_round","description":"closed"})"));
                else
                    done (200, json (R"({"ok":true,"status":"idle"})"));
            });

            int changes = 0;
            battle.onChanged = [&] { ++changes; };

            battle.search ("dyn");
            expect (battle.getState().stage == OnlineBattle::Stage::playing);
            expectEquals (lastBody["family"].toString(), juce::String ("dyn"));

            auto* f = new juce::DynamicObject();
            f->setProperty ("chosen", 1);
            f->setProperty ("correct", 1);
            battle.answer (juce::var (f));
            expectEquals (lastBody["matchId"].toString(), juce::String ("m9"));
            expectEquals ((int) lastBody["round"], 0);
            expect (battle.getState().stage == OnlineBattle::Stage::playing,
                    "a round that closed a moment earlier does not end the battle");

            battle.cancel();
            expect (calls.contains ("POST /api/abctrain/battle/leave"));
            expect (battle.getState().stage == OnlineBattle::Stage::idle);
            expect (changes >= 2);
        }
    }
};

static OnlineBattleTest onlineBattleTest;

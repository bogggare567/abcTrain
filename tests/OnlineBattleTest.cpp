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
        beginTest ("a room from the server: players, you, the leader, the round");
        {
            const auto s = OnlineBattle::parse (json (R"({"ok":true,"status":"playing","matchId":"m1","family":"freq","game":8,"rounds":10,"you":1,
                "players":[{"nick":"alpha","country":"RU","rating":1520,"hp":64,"alive":true,"score":3},
                           {"nick":"bravo","country":"RU","rating":1500,"hp":82.4,"alive":true,"score":4},
                           {"nick":"charlie","country":"KZ","rating":1480,"hp":0,"alive":false,"score":1}],
                "history":[{"n":0,"voided":false,"errors":[2.6,0.4,3],"hp":[82.4,100,80]},
                           {"n":1,"voided":true,"errors":[0,0,0],"hp":[82.4,100,80]}],
                "round":{"n":2,"seed":2147483000,"level":6,"startsInMs":0,"deadlineInMs":41000,"answered":false,"answeredCount":2}})"), {});

            expect (s.stage == OnlineBattle::Stage::playing);
            expectEquals (s.game, 8);
            expectEquals ((int) s.players.size(), 3);
            expectEquals (s.you, 1);
            expectEquals (s.opponentNick, juce::String ("alpha"), "the leader: the strongest opponent");
            expectWithinAbsoluteError (s.hpYou, 82.4f, 1.0e-4f);
            expectEquals (s.hpThem, 64.0f);
            expectEquals (s.aliveCount, 2);
            expectWithinAbsoluteError (s.history[0].youError, 0.4f, 1.0e-6f);
            expectWithinAbsoluteError (s.history[0].themError, 2.6f, 1.0e-6f, "the best of the others");
            expectEquals (s.history[0].hpYou, 100.0f);
            expectWithinAbsoluteError (s.history[0].hpThem, 82.4f, 1.0e-4f);
            expect (s.history[1].voided && s.history[1].youError == 0.0f);
            expect (s.round.seed == 2147483000LL, "a seed near 2^31 survives the JSON");
            expect (s.round.opponentAnswered && ! s.round.answered);
        }

        beginTest ("a room found: accept window");
        {
            const auto s = OnlineBattle::parse (json (R"({"ok":true,"status":"ready","family":"dyn","checkId":"c1","deadlineInMs":12000,"members":3,"accepted":1,"max":6,"youAccepted":null})"), {});
            expect (s.stage == OnlineBattle::Stage::ready);
            expectEquals (s.checkId, juce::String ("c1"));
            expectEquals (s.members, 3);
            expectEquals (s.youAccepted, -1);
            const auto t = OnlineBattle::parse (json (R"({"ok":true,"status":"ready","youAccepted":true})"), {});
            expectEquals (t.youAccepted, 1);
        }

        beginTest ("the result: Decibelo delta, forfeit");
        {
            const auto s = OnlineBattle::parse (json (R"({"ok":true,"status":"finished","matchId":"m1","you":0,"history":[],
                "players":[{"nick":"alpha","hp":100,"alive":true},{"nick":"bravo","hp":0,"alive":false}],"place":1,
                "outcome":"won","forfeit":"them","delta":16,"rating":1516,"recorded":true})"), {});
            expectEquals (s.place, 1);
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
                    done (200, json (R"({"ok":true,"status":"playing","matchId":"m9","game":1,"you":0,"players":[{"nick":"a","hp":100,"alive":true},{"nick":"b","hp":100,"alive":true}],"round":{"n":0,"seed":5,"level":4,"startsInMs":2000}})"));
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

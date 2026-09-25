#include <juce_gui_basics/juce_gui_basics.h>
#include "Source/LocalRoom.h"
#include "Source/SeminarHost.h"
#include "Source/InviteListComponent.h"
#include "Source/GameManager.h"
#include "shared/ui/QrCode.h"

// The local seminar room: its protocol end to end (join, vote, reveal,
// scores), the pieces around it (QR codes, the invite list's paste), and a
// real socket round trip on localhost.
namespace
{
    LocalRoom::Response call (LocalRoom& room, const juce::String& method, const juce::String& target,
                              const juce::String& body = {})
    {
        LocalRoom::Request r;
        int length = 0;
        LocalRoom::parseHead (method + " " + target + " HTTP/1.1\r\nHost: x\r\nContent-Length: "
                                  + juce::String (body.getNumBytesAsUTF8()),
                              r, length);
        r.body = body;
        return room.handleRequest (r);
    }

    juce::var jsonOf (const LocalRoom::Response& r)
    {
        return juce::JSON::parse (r.body.toString());
    }
}

class LocalRoomTest : public juce::UnitTest
{
public:
    LocalRoomTest() : juce::UnitTest ("LocalRoom", "Live") {}

    void runTest() override
    {
        beginTest ("the page is served, with the strings substituted and no external request");
        {
            LocalRoom room;
            room.configure ("Test", false, {});
            auto strings = new juce::DynamicObject();
            strings->setProperty ("join", juce::String::fromUTF8 ("Войти</script>"));
            room.setPageStrings (juce::var (strings));

            const auto r = call (room, "GET", "/");
            const auto html = r.body.toString();
            expectEquals (r.status, 200);
            expect (r.contentType.startsWith ("text/html"));
            expect (html.contains ("\"join\""));
            expect (! html.contains (juce::String::fromUTF8 ("Войти</script>")), "a translation must not be able to close the script");
            expect (! html.contains ("https://"), "the room has no internet: nothing may come from outside");
            expectEquals (call (room, "GET", "/nothing").status, 404);
        }

        beginTest ("anyone with the room code joins; a wrong code or no name does not");
        {
            LocalRoom room;
            room.configure ("Test", false, {});
            const auto code = room.getRoomCode();
            expectEquals (code.length(), 4);

            expectEquals (call (room, "POST", "/api/join", "{\"name\":\"Ivan\",\"room\":\"0000\"}").status, 403);
            expectEquals (call (room, "POST", "/api/join", "{\"name\":\"  \",\"room\":\"" + code + "\"}").status, 400);

            const auto ok = jsonOf (call (room, "POST", "/api/join", "{\"name\":\"Ivan <b>\",\"room\":\"" + code + "\"}"));
            expect (ok.getProperty ("token", {}).toString().isNotEmpty());
            expectEquals (ok.getProperty ("name", {}).toString(), juce::String ("Ivan b"));
            expectEquals ((int) room.snapshot().players.size(), 1);

            // The same phone coming back keeps its seat.
            const auto again = jsonOf (call (room, "POST", "/api/join", "{\"token\":\"" + ok.getProperty ("token", {}).toString() + "\"}"));
            expectEquals (again.getProperty ("token", {}).toString(), ok.getProperty ("token", {}).toString());
            expectEquals ((int) room.snapshot().players.size(), 1);
        }

        beginTest ("list only: the personal code is the way in, the name comes from the list, one seat per code");
        {
            LocalRoom room;
            room.configure ("Test", true, { { "Maria", "m@school.ru", "4821" }, { "", "kim@school.ru", "1937" } });

            expectEquals (call (room, "POST", "/api/join", "{\"code\":\"1111\"}").status, 403);

            const auto maria = jsonOf (call (room, "POST", "/api/join", "{\"code\":\"4821\",\"name\":\"Hacker\"}"));
            expectEquals (maria.getProperty ("name", {}).toString(), juce::String ("Maria"));

            const auto kim = jsonOf (call (room, "POST", "/api/join", "{\"code\":\"1937\"}"));
            expectEquals (kim.getProperty ("name", {}).toString(), juce::String ("kim"));

            // Her second phone: the seat moves, the room does not grow.
            call (room, "POST", "/api/join", "{\"code\":\"4821\"}");
            expectEquals ((int) room.snapshot().players.size(), 2);
        }

        beginTest ("a discrete round: votes only while open, scored on reveal, changeable until then");
        {
            LocalRoom room;
            room.configure ("Test", false, {});
            const auto code = room.getRoomCode();
            const auto a = jsonOf (call (room, "POST", "/api/join", "{\"name\":\"A\",\"room\":\"" + code + "\"}")).getProperty ("token", {}).toString();
            const auto b = jsonOf (call (room, "POST", "/api/join", "{\"name\":\"B\",\"room\":\"" + code + "\"}")).getProperty ("token", {}).toString();

            expectEquals (call (room, "POST", "/api/vote", "{\"token\":\"" + a + "\",\"choice\":1}").status, 409, "no question yet");

            LocalRoom::Question q;
            q.round = 1;
            q.totalRounds = 3;
            q.exercise = "Guess the Reverb";
            q.choices = { "Room", "Hall", "Plate" };
            room.ask (q);

            expectEquals (call (room, "POST", "/api/vote", "{\"token\":\"" + a + "\",\"choice\":7}").status, 400);
            expectEquals (call (room, "POST", "/api/vote", "{\"token\":\"nobody\",\"choice\":1}").status, 403);
            expectEquals (call (room, "POST", "/api/vote", "{\"token\":\"" + a + "\",\"choice\":0}").status, 200);
            expectEquals (call (room, "POST", "/api/vote", "{\"token\":\"" + a + "\",\"choice\":2}").status, 200);   // changed her mind
            expectEquals (call (room, "POST", "/api/vote", "{\"token\":\"" + b + "\",\"choice\":1}").status, 200);

            const auto state = jsonOf (call (room, "GET", "/api/state?t=" + a));
            expectEquals (state.getProperty ("phase", {}).toString(), juce::String ("question"));
            expectEquals (state["question"]["choices"].size(), 3);
            expectEquals ((int) state.getProperty ("answered", 0), 2);

            room.reveal ({ 2, -1.0f, "Plate" });
            expectEquals (call (room, "POST", "/api/vote", "{\"token\":\"" + b + "\",\"choice\":2}").status, 409, "too late");

            const auto snap = room.snapshot();
            expectEquals (snap.right, 1);
            expectEquals (snap.players.front().name, juce::String ("A"));
            expectEquals (snap.players.front().score, 1);

            const auto after = jsonOf (call (room, "GET", "/api/state?t=" + b));
            expect (! (bool) after["answer"]["right"]);
            expectEquals (after["answer"]["label"].toString(), juce::String ("Plate"));
        }

        beginTest ("a continuous round is right within the tolerance");
        {
            LocalRoom room;
            room.configure ("Test", false, {});
            const auto code = room.getRoomCode();
            const auto a = jsonOf (call (room, "POST", "/api/join", "{\"name\":\"A\",\"room\":\"" + code + "\"}")).getProperty ("token", {}).toString();
            const auto b = jsonOf (call (room, "POST", "/api/join", "{\"name\":\"B\",\"room\":\"" + code + "\"}")).getProperty ("token", {}).toString();

            LocalRoom::Question q;
            q.continuous = true;
            q.tolerance = 0.05f;
            for (int i = 0; i <= 100; ++i)
                q.scaleLabels.add (juce::String (i));
            room.ask (q);

            expectEquals (call (room, "POST", "/api/vote", "{\"token\":\"" + a + "\",\"value\":1.5}").status, 400);
            call (room, "POST", "/api/vote", "{\"token\":\"" + a + "\",\"value\":0.53}");
            call (room, "POST", "/api/vote", "{\"token\":\"" + b + "\",\"value\":0.7}");
            room.reveal ({ -1, 0.5f, "1 kHz" });

            expectEquals (room.snapshot().right, 1);
            expectEquals ((int) room.snapshot().votes.size(), 2);
        }

        beginTest ("finish tells each phone its place");
        {
            LocalRoom room;
            room.configure ("Test", false, {});
            const auto code = room.getRoomCode();
            const auto a = jsonOf (call (room, "POST", "/api/join", "{\"name\":\"A\",\"room\":\"" + code + "\"}")).getProperty ("token", {}).toString();
            const auto b = jsonOf (call (room, "POST", "/api/join", "{\"name\":\"B\",\"room\":\"" + code + "\"}")).getProperty ("token", {}).toString();
            LocalRoom::Question q;
            q.choices = { "x", "y" };
            room.ask (q);
            call (room, "POST", "/api/vote", "{\"token\":\"" + b + "\",\"choice\":1}");
            room.reveal ({ 1, -1.0f, "y" });
            room.finish();

            expectEquals ((int) jsonOf (call (room, "GET", "/api/state?t=" + b)).getProperty ("place", 0), 1);
            expectEquals ((int) jsonOf (call (room, "GET", "/api/state?t=" + a)).getProperty ("place", 0), 2);
        }

        beginTest ("request heads parse; garbage does not");
        {
            LocalRoom::Request r;
            int length = -1;
            expect (LocalRoom::parseHead ("GET /api/state?t=a%20b&x=1 HTTP/1.1\r\nContent-Length: 12", r, length));
            expectEquals (r.path, juce::String ("/api/state"));
            expectEquals (r.query["t"], juce::String ("a b"));
            expectEquals (length, 12);
            expect (! LocalRoom::parseHead ("hello", r, length));
            expect (! LocalRoom::parseHead ("GET nothing HTTP/1.1", r, length));
        }

        beginTest ("a real socket: the room answers on localhost");
        {
            LocalRoom room;
            room.configure ("Socket", false, {});
            juce::String error;

            if (room.open (18930, error))
            {
                juce::StreamingSocket s;
                expect (s.connect ("127.0.0.1", room.getPort(), 2000));
                const juce::String request ("GET /api/state HTTP/1.1\r\nHost: localhost\r\n\r\n");
                s.write (request.toRawUTF8(), (int) request.getNumBytesAsUTF8());

                juce::MemoryBlock got;
                char buffer[1024];
                for (int i = 0; i < 50; ++i)
                {
                    if (s.waitUntilReady (true, 200) != 1)
                        continue;
                    const auto n = s.read (buffer, sizeof (buffer), false);
                    if (n <= 0)
                        break;
                    got.append (buffer, (size_t) n);
                }

                const auto text = got.toString();
                expect (text.startsWith ("HTTP/1.1 200"), text.substring (0, 80));
                expect (text.contains ("\"title\": \"Socket\""), text);
                room.close();
                expect (! room.isOpen());
            }
            else
            {
                logMessage ("no port for the socket test: " + error);
            }
        }

        beginTest ("QR codes: the URL fits, and every code has its finder patterns");
        {
            QrCode qr ("http://192.168.100.200:8930/?r=4821");
            expect (qr.isValid());
            expect (qr.getSize() >= 21 && qr.getSize() <= 41);

            // Finder pattern corners are dark, the separator next to them light.
            expect (qr.isDark (0, 0) && qr.isDark (qr.getSize() - 1, 0) && qr.isDark (0, qr.getSize() - 1));
            expect (! qr.isDark (7, 7));
            expect (qr.toSvg().startsWith ("<svg"));
        }

        beginTest ("a seminar host asks real exercise rounds and reveals the game's own answer");
        {
            GameManager games;
            games.prepare ({ 44100.0, 512, 2 });
            int active = 0, rounds = 0;
            bool playing = false, processed = false;

            SeminarHost::Hooks hooks;
            hooks.activeGameIndex = [&] { return active; };
            hooks.game = [&] (int i) -> Game& { return games.getGame (i); };
            hooks.selectGame = [&] (int i) { active = i; games.setActiveGameIndex (i); };
            hooks.startRound = [&] { ++rounds; games.getActiveGame().newRound(); };
            hooks.setSound = [&] (bool p, bool pr) { playing = p; processed = pr; };
            hooks.localisedName = [] (const Game& g) { return g.getName(); };
            hooks.localisedPrompt = [] (const Game& g) { return g.getInstructions(); };

            SeminarHost host (std::move (hooks));
            host.configure ("T", 0b0101, 3, false, {});   // frequency and space
            host.start();
            expect (host.getStage() == SeminarHost::Stage::closed, "no room, no rounds");

            juce::String error;
            expect (host.openRoom (error), error);
            host.start();

            expect (host.getStage() == SeminarHost::Stage::listening);
            expect (playing && ! processed, "the hall hears the clean version first");
            expectEquals (rounds, 1);

            const auto snap = host.getRoom().snapshot();
            expectEquals (snap.question.round, 1);
            expectEquals (snap.question.totalRounds, 3);
            expect (snap.question.continuous ? snap.question.scaleLabels.size() == 101 : snap.question.choices.size() > 1);

            const auto& firstFamily = SeminarHost::gamesOfFamily (0);
            expect (std::find (firstFamily.begin(), firstFamily.end(), active) != firstFamily.end());

            host.reveal();
            expect (host.getStage() == SeminarHost::Stage::revealed);
            expect (processed, "after the reveal the processed version is the lesson");
            expect (host.getRoom().snapshot().answer.label.isNotEmpty());

            host.next();
            const auto& secondFamily = SeminarHost::gamesOfFamily (2);
            expect (std::find (secondFamily.begin(), secondFamily.end(), active) != secondFamily.end(), "families take turns");

            host.reveal();
            host.next();
            host.reveal();
            host.next();
            expect (host.getStage() == SeminarHost::Stage::finished);
            expect (! playing);
            host.closeRoom();
        }

        beginTest ("the invite list takes a pasted spreadsheet, a CSV and a mail header");
        {
            const auto people = InviteListComponent::parsePeople ("Name\tE-mail\nIvan Petrov\tivan@school.ru\nm.sokolova@school.ru;Maria Sokolova\n"
                                                                  "Artyom Kim <a.kim@school.ru>\n\n   \nPavel, zhukov@school.ru");
            expectEquals ((int) people.size(), 4);
            expectEquals (people[0].first, juce::String ("Ivan Petrov"));
            expectEquals (people[0].second, juce::String ("ivan@school.ru"));
            expectEquals (people[1].second, juce::String ("m.sokolova@school.ru"));
            expectEquals (people[1].first, juce::String ("Maria Sokolova"));
            expectEquals (people[2].second, juce::String ("a.kim@school.ru"));
            expectEquals (people[2].first, juce::String ("Artyom Kim"));
            expectEquals (people[3].first, juce::String ("Pavel"));

            juce::Random r (1);
            juce::StringArray codes;
            for (int i = 0; i < 200; ++i)
                codes.add (LocalRoom::makeCode (r, codes));
            expectEquals (codes.size(), 200);
            for (const auto& c : codes)
                expect (c.length() == 4 && c[0] != '0');

            expect (InviteListComponent::looksLikeMail ("a@b.ru"));
            expect (! InviteListComponent::looksLikeMail ("a@b"));
            expect (! InviteListComponent::looksLikeMail ("a b@c.ru"));
        }
    }
};

static LocalRoomTest localRoomTest;

#include "LocalRoom.h"
#include "LocalRoomPage.h"

namespace
{
    constexpr int maxSeats = 300;               // a big hall; beyond this is a stadium, not a seminar
    constexpr int maxBodyBytes = 8 * 1024;
    constexpr int maxHeadBytes = 16 * 1024;
    constexpr juce::uint32 presentWithinMs = 6000;

    juce::String phaseName (LocalRoom::Phase p)
    {
        switch (p)
        {
            case LocalRoom::Phase::lobby:    return "lobby";
            case LocalRoom::Phase::question: return "question";
            case LocalRoom::Phase::revealed: return "revealed";
            case LocalRoom::Phase::finished: return "finished";
        }

        return "lobby";
    }

    juce::String cleanName (juce::String name)
    {
        name = name.removeCharacters ("<>\"\\\r\n\t").trim();
        return name.substring (0, 40);
    }
}

// ---- the listener -----------------------------------------------------

class LocalRoom::Server : public juce::Thread
{
public:
    explicit Server (LocalRoom& r) : juce::Thread ("abcTrain room"), room (r) {}

    ~Server() override
    {
        stop();
    }

    bool start (int firstPort, int& chosen, juce::String& error)
    {
        for (int p = firstPort; p < firstPort + 10; ++p)
        {
            if (listener.createListener (p))
            {
                chosen = p;
                startThread();
                return true;
            }
        }

        error = "no free port " + juce::String (firstPort) + "-" + juce::String (firstPort + 9);
        return false;
    }

    void stop()
    {
        signalThreadShouldExit();
        listener.close();            // unblocks waitForNextConnection
        stopThread (3000);
        pool.removeAllJobs (true, 3000);
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            std::unique_ptr<juce::StreamingSocket> client (listener.waitForNextConnection());

            if (client == nullptr)
            {
                if (! threadShouldExit())
                    wait (50);
                continue;
            }

            pool.addJob (new Connection (room, std::move (client)), true);
        }
    }

private:
    struct Connection : public juce::ThreadPoolJob
    {
        Connection (LocalRoom& r, std::unique_ptr<juce::StreamingSocket> s)
            : juce::ThreadPoolJob ("room request"), room (r), socket (std::move (s)) {}

        JobStatus runJob() override
        {
            juce::MemoryBlock received;
            char buffer[2048];
            int headEnd = -1;

            // A slow phone gets three seconds per read; a silent one is dropped.
            while (headEnd < 0 && (int) received.getSize() < maxHeadBytes)
            {
                if (socket->waitUntilReady (true, 3000) != 1)
                    return jobHasFinished;

                const auto n = socket->read (buffer, sizeof (buffer), false);
                if (n <= 0)
                    return jobHasFinished;

                received.append (buffer, (size_t) n);
                const auto text = juce::String::fromUTF8 ((const char*) received.getData(), (int) received.getSize());
                headEnd = text.indexOf ("\r\n\r\n");
            }

            if (headEnd < 0)
                return jobHasFinished;

            // Byte offsets, not character offsets: the head is ASCII, so the
            // two agree up to the blank line.
            const auto all = juce::String::fromUTF8 ((const char*) received.getData(), (int) received.getSize());
            LocalRoom::Request request;
            int contentLength = 0;

            if (! LocalRoom::parseHead (all.substring (0, headEnd), request, contentLength) || contentLength > maxBodyBytes)
            {
                send (400, "text/plain", "bad request");
                return jobHasFinished;
            }

            const auto bodyStart = (size_t) headEnd + 4;
            juce::MemoryBlock body;
            if (received.getSize() > bodyStart)
                body.append ((const char*) received.getData() + bodyStart, received.getSize() - bodyStart);

            while ((int) body.getSize() < contentLength)
            {
                if (socket->waitUntilReady (true, 3000) != 1)
                    return jobHasFinished;

                const auto n = socket->read (buffer, sizeof (buffer), false);
                if (n <= 0)
                    return jobHasFinished;

                body.append (buffer, (size_t) n);
            }

            request.body = juce::String::fromUTF8 ((const char*) body.getData(), juce::jmin ((int) body.getSize(), contentLength));
            request.remoteAddress = socket->getHostName();

            const auto response = room.handleRequest (request);
            write (response);
            return jobHasFinished;
        }

        void send (int status, const juce::String& type, const juce::String& text)
        {
            LocalRoom::Response r;
            r.status = status;
            r.contentType = type;
            r.body.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
            write (r);
        }

        void write (const LocalRoom::Response& r)
        {
            const auto reason = r.status == 200 ? "OK" : r.status == 404 ? "Not Found"
                              : r.status == 400 ? "Bad Request" : r.status == 403 ? "Forbidden" : "Error";
            juce::String head;
            head << "HTTP/1.1 " << r.status << " " << reason << "\r\n"
                 << "Content-Type: " << r.contentType << "\r\n"
                 << "Content-Length: " << (int) r.body.getSize() << "\r\n"
                 << "Cache-Control: no-store\r\n"
                 << "X-Content-Type-Options: nosniff\r\n"
                 << "Connection: close\r\n\r\n";

            socket->write (head.toRawUTF8(), (int) head.getNumBytesAsUTF8());
            if (r.body.getSize() > 0)
                socket->write (r.body.getData(), (int) r.body.getSize());
            socket->close();
        }

        LocalRoom& room;
        std::unique_ptr<juce::StreamingSocket> socket;
    };

    LocalRoom& room;
    juce::StreamingSocket listener;
    juce::ThreadPool pool { juce::ThreadPoolOptions{}.withNumberOfThreads (4) };
};

// ---- the room ---------------------------------------------------------

LocalRoom::LocalRoom() = default;

LocalRoom::~LocalRoom()
{
    cancelPendingUpdate();
    close();
}

void LocalRoom::configure (const juce::String& newTitle, bool shouldBeListOnly, std::vector<Invitee> newInvitees)
{
    const juce::ScopedLock sl (lock);
    title = newTitle;
    listOnly = shouldBeListOnly;
    invitees = std::move (newInvitees);
    seats.clear();
    phase = Phase::lobby;
    question = {};
    answer = {};
    ++questionSerial;
    roomCode = makeCode (random, {});
}

bool LocalRoom::open (int preferredPort, juce::String& error)
{
    close();

    server = std::make_unique<Server> (*this);
    int chosen = 0;

    if (! server->start (preferredPort, chosen, error))
    {
        server.reset();
        return false;
    }

    port.store (chosen);
    listening.store (true);
    changed();
    return true;
}

void LocalRoom::close()
{
    if (server != nullptr)
        server->stop();

    server.reset();
    listening.store (false);
    port.store (0);
}

juce::String LocalRoom::getRoomCode() const
{
    const juce::ScopedLock sl (lock);
    return roomCode;
}

void LocalRoom::setPageStrings (juce::var strings)
{
    const juce::ScopedLock sl (lock);
    pageStrings = std::move (strings);
}

juce::String LocalRoom::makeCode (juce::Random& r, const juce::StringArray& taken)
{
    for (;;)
    {
        const auto code = juce::String (r.nextInt ({ 1000, 10000 }));
        if (! taken.contains (code))
            return code;
    }
}

void LocalRoom::ask (const Question& q)
{
    {
        const juce::ScopedLock sl (lock);
        question = q;
        answer = {};
        phase = Phase::question;
        ++questionSerial;

        for (auto& s : seats)
        {
            s.hasVote = false;
            s.vote = -1.0f;
            s.lastRight = false;
        }
    }

    changed();
}

void LocalRoom::reveal (const Answer& a)
{
    {
        const juce::ScopedLock sl (lock);

        if (phase != Phase::question)
            return;

        answer = a;
        phase = Phase::revealed;

        for (auto& s : seats)
        {
            if (! s.hasVote)
            {
                s.lastRight = false;
                continue;
            }

            s.lastRight = question.continuous
                        ? std::abs (s.vote - a.value) <= juce::jmax (0.005f, question.tolerance)
                        : juce::roundToInt (s.vote) == a.choice;

            if (s.lastRight)
                ++s.score;
        }
    }

    changed();
}

void LocalRoom::finish()
{
    {
        const juce::ScopedLock sl (lock);
        phase = Phase::finished;
    }

    changed();
}

void LocalRoom::backToLobby()
{
    {
        const juce::ScopedLock sl (lock);
        phase = Phase::lobby;
        question = {};
        answer = {};

        for (auto& s : seats)
        {
            s.score = 0;
            s.hasVote = false;
        }
    }

    changed();
}

LocalRoom::Snapshot LocalRoom::snapshot() const
{
    const juce::ScopedLock sl (lock);
    Snapshot s;
    s.open = listening.load();
    s.port = port.load();
    s.title = title;
    s.roomCode = roomCode;
    s.listOnly = listOnly;
    s.invited = (int) invitees.size();
    s.phase = phase;
    s.question = question;
    s.answer = answer;

    const auto now = juce::Time::getMillisecondCounter();

    for (const auto& seat : seats)
    {
        Player p;
        p.name = seat.name;
        p.score = seat.score;
        p.voted = seat.hasVote;
        p.lastRight = seat.lastRight;
        p.present = now - seat.lastSeenMs < presentWithinMs;
        s.players.push_back (p);

        if (seat.hasVote)
        {
            s.votes.push_back (seat.vote);
            ++s.answered;

            if (phase == Phase::revealed && seat.lastRight)
                ++s.right;
        }
    }

    std::stable_sort (s.players.begin(), s.players.end(),
                      [] (const Player& a, const Player& b) { return a.score > b.score; });
    return s;
}

void LocalRoom::handleAsyncUpdate()
{
    if (onChanged != nullptr)
        onChanged();
}

// ---- HTTP -------------------------------------------------------------

bool LocalRoom::parseHead (const juce::String& head, Request& request, int& contentLength)
{
    const auto lines = juce::StringArray::fromLines (head);

    if (lines.isEmpty())
        return false;

    const auto parts = juce::StringArray::fromTokens (lines[0], " ", "");

    if (parts.size() < 2 || ! parts[1].startsWithChar ('/'))
        return false;

    request.method = parts[0].toUpperCase();
    const auto target = parts[1];
    request.path = target.upToFirstOccurrenceOf ("?", false, false);
    request.query.clear();

    for (const auto& pair : juce::StringArray::fromTokens (target.fromFirstOccurrenceOf ("?", false, false), "&", ""))
    {
        if (pair.isEmpty())
            continue;

        request.query.set (juce::URL::removeEscapeChars (pair.upToFirstOccurrenceOf ("=", false, false).replaceCharacter ('+', ' ')),
                           juce::URL::removeEscapeChars (pair.fromFirstOccurrenceOf ("=", false, false).replaceCharacter ('+', ' ')));
    }

    contentLength = 0;

    for (int i = 1; i < lines.size(); ++i)
        if (lines[i].startsWithIgnoreCase ("content-length:"))
            contentLength = lines[i].fromFirstOccurrenceOf (":", false, false).trim().getIntValue();

    return contentLength >= 0;
}

LocalRoom::Response LocalRoom::json (int status, const juce::var& v) const
{
    Response r;
    r.status = status;
    const auto text = juce::JSON::toString (v, true);
    r.body.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
    return r;
}

LocalRoom::Response LocalRoom::page() const
{
    juce::String strings;
    {
        const juce::ScopedLock sl (lock);
        strings = juce::JSON::toString (pageStrings.isObject() ? pageStrings : juce::var (new juce::DynamicObject()), true);
    }

    // "</" inside a <script> would end it; the strings are ours, but a
    // translation is still text somebody typed.
    strings = strings.replace ("</", "<\\/");

    Response r;
    r.contentType = "text/html; charset=utf-8";
    const auto html = LocalRoomPage::html().replace ("/*STRINGS*/{}", strings);
    r.body.append (html.toRawUTF8(), html.getNumBytesAsUTF8());
    return r;
}

LocalRoom::Seat* LocalRoom::seatFor (const juce::String& token)
{
    if (token.isEmpty())
        return nullptr;

    for (auto& s : seats)
        if (s.token == token)
            return &s;

    return nullptr;
}

juce::var LocalRoom::stateFor (const Seat* seat) const
{
    auto* o = new juce::DynamicObject();
    juce::var result (o);
    o->setProperty ("title", title);
    o->setProperty ("listOnly", listOnly);
    o->setProperty ("phase", phaseName (phase));
    o->setProperty ("serial", questionSerial);
    o->setProperty ("players", (int) seats.size());
    o->setProperty ("joined", seat != nullptr);

    if (seat == nullptr)
        return result;

    o->setProperty ("name", seat->name);
    o->setProperty ("score", seat->score);

    int answered = 0;
    for (const auto& s : seats)
        answered += s.hasVote ? 1 : 0;

    o->setProperty ("answered", answered);

    if (phase == Phase::question || phase == Phase::revealed)
    {
        auto* q = new juce::DynamicObject();
        q->setProperty ("round", question.round);
        q->setProperty ("total", question.totalRounds);
        q->setProperty ("exercise", question.exercise);
        q->setProperty ("prompt", question.prompt);
        q->setProperty ("continuous", question.continuous);

        juce::Array<juce::var> choices, labels, marks;
        for (const auto& c : question.choices) choices.add (c);
        for (const auto& l : question.scaleLabels) labels.add (l);

        for (const auto& m : question.marks)
        {
            auto* mo = new juce::DynamicObject();
            mo->setProperty ("at", m.position);
            mo->setProperty ("label", m.label);
            marks.add (juce::var (mo));
        }

        q->setProperty ("choices", choices);
        q->setProperty ("labels", labels);
        q->setProperty ("marks", marks);
        o->setProperty ("question", juce::var (q));

        if (seat->hasVote)
            o->setProperty ("vote", seat->vote);
    }

    if (phase == Phase::revealed)
    {
        auto* a = new juce::DynamicObject();
        a->setProperty ("choice", answer.choice);
        a->setProperty ("value", answer.value);
        a->setProperty ("label", answer.label);
        a->setProperty ("right", seat->lastRight);
        a->setProperty ("voted", seat->hasVote);
        o->setProperty ("answer", juce::var (a));
    }

    if (phase == Phase::finished)
    {
        int place = 1;
        for (const auto& s : seats)
            if (s.score > seat->score)
                ++place;

        o->setProperty ("place", place);
    }

    return result;
}

LocalRoom::Response LocalRoom::join (const juce::var& body)
{
    const auto code = body.getProperty ("code", {}).toString().retainCharacters ("0123456789");
    const auto room = body.getProperty ("room", {}).toString().retainCharacters ("0123456789");
    const auto token = body.getProperty ("token", {}).toString();

    const juce::ScopedLock sl (lock);

    // The same phone coming back (reload, a sleep) keeps its seat.
    if (auto* seat = seatFor (token))
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("token", seat->token);
        o->setProperty ("name", seat->name);
        return json (200, juce::var (o));
    }

    juce::String name, inviteCode;

    if (listOnly)
    {
        for (const auto& person : invitees)
            if (person.code == code)
            {
                name = person.name.isNotEmpty() ? person.name : person.mail.upToFirstOccurrenceOf ("@", false, false);
                inviteCode = person.code;
            }

        if (inviteCode.isEmpty())
            return json (403, juce::JSON::parse ("{\"error\":\"badCode\"}"));

        // One seat per code: a second phone with the same code takes the
        // seat over (the first one was probably the same person).
        for (auto& s : seats)
            if (s.inviteCode == inviteCode)
            {
                s.token = juce::Uuid().toDashedString();
                auto* o = new juce::DynamicObject();
                o->setProperty ("token", s.token);
                o->setProperty ("name", s.name);
                return json (200, juce::var (o));
            }
    }
    else
    {
        if (room != roomCode)
            return json (403, juce::JSON::parse ("{\"error\":\"badRoom\"}"));

        name = cleanName (body.getProperty ("name", {}).toString());

        if (name.isEmpty())
            return json (400, juce::JSON::parse ("{\"error\":\"nameNeeded\"}"));
    }

    if ((int) seats.size() >= maxSeats)
        return json (403, juce::JSON::parse ("{\"error\":\"full\"}"));

    Seat seat;
    seat.token = juce::Uuid().toDashedString();
    seat.name = name;
    seat.inviteCode = inviteCode;
    seat.lastSeenMs = juce::Time::getMillisecondCounter();
    seats.push_back (seat);

    auto* o = new juce::DynamicObject();
    o->setProperty ("token", seat.token);
    o->setProperty ("name", seat.name);
    changed();
    return json (200, juce::var (o));
}

LocalRoom::Response LocalRoom::vote (const juce::var& body)
{
    const juce::ScopedLock sl (lock);
    auto* seat = seatFor (body.getProperty ("token", {}).toString());

    if (seat == nullptr)
        return json (403, juce::JSON::parse ("{\"error\":\"notJoined\"}"));

    // A vote is taken only while the question is open; after the reveal a
    // late tap would rewrite history.
    if (phase != Phase::question)
        return json (409, juce::JSON::parse ("{\"error\":\"closed\"}"));

    if (question.continuous)
    {
        const auto v = (float) (double) body.getProperty ("value", -1.0);
        if (! (v >= 0.0f && v <= 1.0f))
            return json (400, juce::JSON::parse ("{\"error\":\"badValue\"}"));
        seat->vote = v;
    }
    else
    {
        const auto c = (int) body.getProperty ("choice", -1);
        if (c < 0 || c >= question.choices.size())
            return json (400, juce::JSON::parse ("{\"error\":\"badChoice\"}"));
        seat->vote = (float) c;
    }

    seat->hasVote = true;
    seat->lastSeenMs = juce::Time::getMillisecondCounter();
    changed();
    return json (200, stateFor (seat));
}

LocalRoom::Response LocalRoom::handleRequest (const Request& request)
{
    if (request.method == "GET" && (request.path == "/" || request.path == "/index.html"))
        return page();

    if (request.method == "GET" && request.path == "/api/state")
    {
        const juce::ScopedLock sl (lock);
        auto* seat = seatFor (request.query["t"]);

        if (seat != nullptr)
            seat->lastSeenMs = juce::Time::getMillisecondCounter();

        return json (200, stateFor (seat));
    }

    if (request.method == "POST" && (request.path == "/api/join" || request.path == "/api/vote"))
    {
        const auto body = juce::JSON::parse (request.body);

        if (! body.isObject())
            return json (400, juce::JSON::parse ("{\"error\":\"badJson\"}"));

        return request.path == "/api/join" ? join (body) : vote (body);
    }

    if (request.path == "/favicon.ico")
    {
        Response r;
        r.status = 404;
        return r;
    }

    Response r;
    r.status = 404;
    r.contentType = "text/plain; charset=utf-8";
    r.body.append ("not found", 9);
    return r;
}

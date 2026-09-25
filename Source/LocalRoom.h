#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <vector>

// A seminar room served by this computer on the venue's network.
//
// The presenter's laptop plays the exercise into the hall; everybody in the
// room answers on a phone. The phones open a page this class serves over
// plain HTTP on the local network - no internet, no account, no app on the
// phone. That is the whole of the "local" Live mode (the canvas «abcTrain
// Live и рейтинги», note "Закрытый семинар").
//
// Shape:
//   - the state (who joined, the question on screen, the votes) lives here,
//     behind one lock, and is read by the projector window and the host
//     panel through snapshot();
//   - handleRequest() turns one HTTP request into one response. It is the
//     whole protocol and needs no socket, so the tests call it directly;
//   - a listener thread accepts connections and hands each to a small
//     thread pool, which reads the request, calls handleRequest() and
//     closes the connection.
//
// Nothing here touches audio or the GUI. onChanged is posted to the message
// thread whenever a phone changes something the host should see.
//
// Why HTTP polling and not WebSockets: a phone checks the state once a
// second. For a seminar that is instant enough, it survives a phone going
// to sleep and waking up, and it is fifty lines instead of a protocol.
class LocalRoom : private juce::AsyncUpdater
{
public:
    LocalRoom();
    ~LocalRoom() override;

    // ---- the room -----------------------------------------------------

    struct Invitee
    {
        juce::String name, mail, code;
    };

    // Before open(): who may come in. listOnly false - anybody who has the
    // room code (it is in the QR code); true - only a personal code from the
    // list, and the name comes from the list.
    void configure (const juce::String& title, bool listOnly, std::vector<Invitee> invitees);

    // Starts serving on the first free port from `preferredPort` up. False,
    // with the reason, when no port could be opened.
    bool open (int preferredPort, juce::String& error);
    void close();
    bool isOpen() const noexcept { return listening.load(); }
    int getPort() const noexcept { return port.load(); }
    juce::String getRoomCode() const;

    // The text the phone page shows, by key ("join", "yourName", ...): the
    // app's language, sent to the phone with the page.
    void setPageStrings (juce::var strings);

    // Four digits, never starting with 0, unique within `taken`.
    static juce::String makeCode (juce::Random&, const juce::StringArray& taken);

    // ---- rounds ---------------------------------------------------------

    struct Mark
    {
        float position = 0.0f;
        juce::String label;
    };

    struct Question
    {
        int round = 0, totalRounds = 0;
        juce::String exercise, prompt;
        bool continuous = false;
        juce::StringArray choices;         // discrete: one button each
        juce::StringArray scaleLabels;     // continuous: the value at 0, 0.01 ... 1
        std::vector<Mark> marks;           // continuous: grid labels under the slider
        float tolerance = 0.0f;            // continuous: |vote - answer| within this is right
    };

    struct Answer
    {
        int choice = -1;        // discrete
        float value = -1.0f;    // continuous, 0..1
        juce::String label;     // what the answer was, in words
    };

    enum class Phase { lobby, question, revealed, finished };

    void ask (const Question&);       // clears the votes, phones switch to it
    void reveal (const Answer&);      // scores the votes, phones see right/wrong
    void finish();                    // phones see their place
    void backToLobby();

    // ---- what the host sees ---------------------------------------------

    struct Player
    {
        juce::String name;
        int score = 0;
        bool voted = false;
        bool lastRight = false;
        bool present = false;    // polled within the last few seconds
    };

    struct Snapshot
    {
        bool open = false;
        int port = 0;
        juce::String title, roomCode;
        bool listOnly = false;
        int invited = 0;
        Phase phase = Phase::lobby;
        Question question;
        Answer answer;
        std::vector<Player> players;     // joined, best score first
        std::vector<float> votes;        // this question: choice index (discrete) or value (continuous)
        int answered = 0;
        int right = 0;                   // after reveal
    };

    Snapshot snapshot() const;

    std::function<void()> onChanged;     // message thread

    // ---- the protocol ---------------------------------------------------

    struct Request
    {
        juce::String method, path;
        juce::StringPairArray query;
        juce::String body;
        juce::String remoteAddress;
    };

    struct Response
    {
        int status = 200;
        juce::String contentType { "application/json; charset=utf-8" };
        juce::MemoryBlock body;
    };

    Response handleRequest (const Request&);

    // Parses the request line and headers of raw HTTP; false on garbage.
    static bool parseHead (const juce::String& head, Request&, int& contentLength);

private:
    void handleAsyncUpdate() override;
    void changed() { triggerAsyncUpdate(); }

    struct Seat
    {
        juce::String token, name, inviteCode;
        int score = 0;
        bool hasVote = false;
        float vote = -1.0f;
        bool lastRight = false;
        juce::uint32 lastSeenMs = 0;
    };

    Seat* seatFor (const juce::String& token);
    juce::var stateFor (const Seat*) const;
    Response json (int status, const juce::var&) const;
    Response join (const juce::var& body);
    Response vote (const juce::var& body);
    Response page() const;

    mutable juce::CriticalSection lock;
    juce::String title, roomCode;
    bool listOnly = false;
    std::vector<Invitee> invitees;
    std::vector<Seat> seats;
    Phase phase = Phase::lobby;
    int questionSerial = 0;
    Question question;
    Answer answer;
    juce::var pageStrings;
    juce::Random random;

    std::atomic<bool> listening { false };
    std::atomic<int> port { 0 };

    class Server;
    std::unique_ptr<Server> server;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LocalRoom)
};

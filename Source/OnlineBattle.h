#pragma once

#include <juce_events/juce_events.h>
#include <functional>
#include <vector>

// A battle against a person, over soundkorb.ru (ADR 048).
//
// The client side of /api/abctrain/battle/*: stand in the queue, poll the
// state, send answers, show the result. It knows nothing about exercises or
// screens - the editor turns its state into rounds - and nothing about HTTP:
// the transport is handed in (LiveAccount::battleCall in the app, a fake
// server in tests/OnlineBattleTest).
//
// Polling, not a socket: a round lasts tens of seconds, half a second of
// latency is invisible in it, and plain HTTPS through the same nginx as the
// rest of the site needs no new port, no proxy config and no reconnect logic.
class OnlineBattle : private juce::Timer
{
public:
    using Done = std::function<void (int status, const juce::var& json)>;
    using Transport = std::function<void (const juce::String& method, const juce::String& path,
                                          const juce::var& body, Done done)>;

    explicit OnlineBattle (Transport);
    ~OnlineBattle() override;

    // ready: a room was found and waits for "Accept" (like CS: as soon as
    // two are searching, up to six; ADR 050).
    enum class Stage { idle, searching, ready, playing, finished, failed };

    struct RoundResult
    {
        int n = 0;
        bool you = false, them = false, voided = false;
        float youError = 0.0f, themError = 0.0f;   // relative, as Game::answerErrorRelative (ADR 049); them = the leader
        float hpYou = 100.0f, hpThem = 100.0f;     // after the round; them = the strongest opponent
    };

    struct Player
    {
        juce::String nick, country;
        int rating = 0, score = 0;
        float hp = 100.0f;
        bool alive = true;
    };

    struct Round
    {
        int n = -1;
        juce::int64 seed = 0;
        int level = 1;
        int startsInMs = 0, deadlineInMs = 0;
        bool answered = false, opponentAnswered = false;
    };

    struct State
    {
        Stage stage = Stage::idle;
        juce::String family;
        int waitingMs = 0, inQueue = 0;

        // ready
        juce::String checkId;
        int deadlineInMs = 0, members = 0, accepted = 0, maxPlayers = 6;
        int youAccepted = -1;   // -1 not yet, 0 declined, 1 accepted

        juce::String matchId;
        int game = -1, rounds = 10;
        std::vector<Player> players;   // everyone in the room, in the server's order
        int you = 0;                   // index into players
        int aliveCount = 0;
        int place = 0;                 // when finished: 1 = won
        juce::String opponentNick, opponentCountry;
        int opponentRating = 0, yourRating = 0;
        int scoreYou = 0, scoreThem = 0;
        float hpYou = 100.0f, hpThem = 100.0f;
        std::vector<RoundResult> history;
        Round round;

        juce::String outcome;    // won / lost / draw
        juce::String forfeit;    // "", "you", "them"
        double delta = 0.0;
        int rating = 0;
        bool recorded = false;

        juce::String errorCode, errorText;   // from the server, or "offline"
    };

    // Parses one server answer. Static and pure, so the protocol is tested
    // without a network.
    static State parse (const juce::var& json, const State& previous);

    void search (const juce::String& family);
    void accept (bool yes);   // the room found: in or out
    void cancel();     // out of the queue - or, mid-battle, give up
    void answer (const juce::var& fields);   // matchId and round are added here
    void reset();      // after the result was shown: back to idle, polling stops

    const State& getState() const noexcept { return state; }
    bool isActive() const noexcept
    {
        return state.stage == Stage::searching || state.stage == Stage::ready || state.stage == Stage::playing;
    }

    std::function<void()> onChanged;

    static constexpr int searchPollMs = 1000;
    static constexpr int playPollMs = 500;
    static constexpr int failuresBeforeGivingUp = 12;   // ~6-12 s without an answer

private:
    void timerCallback() override;
    void poll();
    void take (int status, const juce::var& json);
    void setStage (Stage);

    Transport transport;
    State state;
    bool inFlight = false;
    int failures = 0;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OnlineBattle)
};

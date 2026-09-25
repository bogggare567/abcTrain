#include "OnlineBattle.h"

OnlineBattle::OnlineBattle (Transport t) : transport (std::move (t)) {}

OnlineBattle::~OnlineBattle()
{
    *alive = false;
    stopTimer();
}

OnlineBattle::State OnlineBattle::parse (const juce::var& json, const State& previous)
{
    State s;
    const auto status = json["status"].toString();

    if (json["ok"].isBool() && ! (bool) json["ok"])
    {
        s = previous;
        s.errorCode = json["error"].toString();
        s.errorText = json["description"].toString();
        return s;
    }

    s.stage = status == "waiting"  ? Stage::searching
            : status == "playing"  ? Stage::playing
            : status == "finished" ? Stage::finished
            : status == "abandoned" ? Stage::failed
                                    : Stage::idle;

    s.family = json["family"].toString();
    s.waitingMs = (int) json["waitingMs"];
    s.inQueue = (int) json["inQueue"];

    s.matchId = json["matchId"].toString();
    s.game = json.hasProperty ("game") ? (int) json["game"] : -1;
    s.rounds = json.hasProperty ("rounds") ? (int) json["rounds"] : 7;
    s.opponentNick = json["opponent"]["nick"].toString();
    s.opponentCountry = json["opponent"]["country"].toString();
    s.opponentRating = (int) json["opponent"]["rating"];
    s.yourRating = (int) json["you"]["rating"];

    if (const auto* score = json["score"].getArray(); score != nullptr && score->size() == 2)
    {
        s.scoreYou = (int) score->getReference (0);
        s.scoreThem = (int) score->getReference (1);
    }

    if (const auto* history = json["history"].getArray())
        for (const auto& h : *history)
            s.history.push_back ({ (int) h["n"], (bool) h["you"], (bool) h["them"], (bool) h["voided"] });

    if (const auto& r = json["round"]; r.isObject())
    {
        s.round.n = (int) r["n"];
        s.round.seed = (juce::int64) r["seed"];
        s.round.level = juce::jlimit (1, 10, (int) r["level"]);
        s.round.startsInMs = (int) r["startsInMs"];
        s.round.deadlineInMs = (int) r["deadlineInMs"];
        s.round.answered = (bool) r["answered"];
        s.round.opponentAnswered = (bool) r["opponentAnswered"];
    }

    s.outcome = json["outcome"].toString();
    s.forfeit = json["forfeit"].isString() ? json["forfeit"].toString() : juce::String();
    s.delta = (double) json["delta"];
    s.rating = (int) json["rating"];
    s.recorded = (bool) json["recorded"];

    if (s.stage == Stage::failed)
        s.errorCode = "abandoned";

    return s;
}

void OnlineBattle::setStage (Stage stage)
{
    state.stage = stage;

    if (stage == Stage::searching)
        startTimer (searchPollMs);
    else if (stage == Stage::playing)
        startTimer (playPollMs);
    else
        stopTimer();
}

void OnlineBattle::take (int status, const juce::var& json)
{
    inFlight = false;

    if (status == 0 || status >= 500)
    {
        // No answer: keep the last state and try again, but not forever.
        if (++failures >= failuresBeforeGivingUp)
        {
            state.errorCode = "offline";
            state.errorText = {};
            setStage (Stage::failed);
            if (onChanged != nullptr) onChanged();
        }
        return;
    }

    failures = 0;
    auto next = parse (json, state);

    // A refused answer (the round closed a moment earlier) is not the
    // battle ending: keep playing and let the next poll bring the state.
    if (next.errorCode.isNotEmpty() && state.stage == Stage::playing
        && (next.errorCode == "wrong_round" || next.errorCode == "too_early"))
    {
        return;
    }

    const auto wasError = next.errorCode.isNotEmpty() && ! (next.stage == Stage::failed);
    state = next;

    if (wasError)
        setStage (Stage::failed);
    else
        setStage (state.stage);

    if (onChanged != nullptr)
        onChanged();
}

void OnlineBattle::poll()
{
    if (inFlight)
        return;

    inFlight = true;
    transport ("GET", "/api/abctrain/battle/state", {}, [flag = alive, this] (int status, const juce::var& json)
    {
        if (*flag)
            take (status, json);
    });
}

void OnlineBattle::timerCallback()
{
    poll();
}

void OnlineBattle::search (const juce::String& family)
{
    state = {};
    state.stage = Stage::searching;
    state.family = family;
    failures = 0;

    auto* body = new juce::DynamicObject();
    body->setProperty ("family", family);

    // Before the request: a transport may answer at once (tests do), and
    // its answer must land on top of "searching", not under it.
    setStage (Stage::searching);
    if (onChanged != nullptr)
        onChanged();

    inFlight = true;
    transport ("POST", "/api/abctrain/battle/queue", juce::var (body), [flag = alive, this] (int status, const juce::var& json)
    {
        if (*flag)
            take (status, json);
    });
}

void OnlineBattle::cancel()
{
    const auto wasActive = isActive();
    stopTimer();
    state = {};
    inFlight = false;

    if (wasActive)
        transport ("POST", "/api/abctrain/battle/leave", juce::var (new juce::DynamicObject()), [] (int, const juce::var&) {});

    if (onChanged != nullptr)
        onChanged();
}

void OnlineBattle::answer (const juce::var& fields)
{
    if (state.stage != Stage::playing)
        return;

    auto* body = new juce::DynamicObject();
    if (auto* f = fields.getDynamicObject())
        for (const auto& p : f->getProperties())
            body->setProperty (p.name, p.value);

    body->setProperty ("matchId", state.matchId);
    body->setProperty ("round", state.round.n);
    state.round.answered = true;

    // Not through inFlight: an answer must not wait behind a poll.
    transport ("POST", "/api/abctrain/battle/answer", juce::var (body), [flag = alive, this] (int status, const juce::var& json)
    {
        if (*flag && status != 0)
            take (status, json);
    });
}

void OnlineBattle::reset()
{
    stopTimer();
    state = {};
    inFlight = false;
    failures = 0;
}

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
            : status == "ready"    ? Stage::ready
            : status == "playing"  ? Stage::playing
            : status == "finished" ? Stage::finished
            : status == "abandoned" ? Stage::failed
                                    : Stage::idle;

    s.family = json["family"].toString();
    s.waitingMs = (int) json["waitingMs"];
    s.inQueue = (int) json["inQueue"];

    s.checkId = json["checkId"].toString();
    s.deadlineInMs = (int) json["deadlineInMs"];
    s.members = (int) json["members"];
    s.accepted = (int) json["accepted"];
    s.maxPlayers = json.hasProperty ("max") ? (int) json["max"] : 6;
    s.youAccepted = json["youAccepted"].isBool() ? ((bool) json["youAccepted"] ? 1 : 0) : -1;

    s.matchId = json["matchId"].toString();
    s.game = json.hasProperty ("game") ? (int) json["game"] : -1;
    s.rounds = json.hasProperty ("rounds") ? (int) json["rounds"] : 10;
    s.you = (int) json["you"];

    if (const auto* players = json["players"].getArray())
        for (const auto& p : *players)
            s.players.push_back ({ p["nick"].toString(), p["country"].toString(), (int) p["rating"], (int) p["score"],
                                   (float) (double) p["hp"], (bool) p["alive"] });

    s.you = juce::jlimit (0, juce::jmax (0, (int) s.players.size() - 1), s.you);

    // The strongest opponent - the one to beat - for a HUD with one name.
    int leader = -1;
    for (int i = 0; i < (int) s.players.size(); ++i)
    {
        if (i == s.you)
            continue;
        s.aliveCount += s.players[(size_t) i].alive ? 1 : 0;
        if (leader < 0 || s.players[(size_t) i].hp > s.players[(size_t) leader].hp)
            leader = i;
    }
    if (! s.players.empty() && s.players[(size_t) s.you].alive)
        ++s.aliveCount;

    if (leader >= 0)
    {
        const auto& l = s.players[(size_t) leader];
        s.opponentNick = l.nick;
        s.opponentCountry = l.country;
        s.opponentRating = l.rating;
        s.hpThem = l.hp;
        s.scoreThem = l.score;
    }

    if (! s.players.empty())
    {
        const auto& me = s.players[(size_t) s.you];
        s.yourRating = me.rating;
        s.hpYou = me.hp;
        s.scoreYou = me.score;
    }

    if (const auto* history = json["history"].getArray())
        for (const auto& h : *history)
        {
            RoundResult r;
            r.n = (int) h["n"];
            r.voided = (bool) h["voided"];
            const auto* errors = h["errors"].getArray();
            const auto* hps = h["hp"].getArray();

            if (errors != nullptr && s.you < errors->size())
            {
                r.youError = (float) (double) errors->getReference (s.you);
                // The opponent's side of the round: the best answer among the others.
                r.themError = 99.0f;
                for (int i = 0; i < errors->size(); ++i)
                    if (i != s.you)
                        r.themError = juce::jmin (r.themError, (float) (double) errors->getReference (i));
                if (r.themError > 50.0f)
                    r.themError = 0.0f;
            }

            if (hps != nullptr && s.you < hps->size())
            {
                r.hpYou = (float) (double) hps->getReference (s.you);
                r.hpThem = 0.0f;
                for (int i = 0; i < hps->size(); ++i)
                    if (i != s.you)
                        r.hpThem = juce::jmax (r.hpThem, (float) (double) hps->getReference (i));
            }

            if (r.voided)
                r.youError = r.themError = 0.0f;

            r.you = ! r.voided && r.youError <= 1.0f;
            r.them = ! r.voided && r.themError <= 1.0f;
            s.history.push_back (r);
        }

    if (const auto& r = json["round"]; r.isObject())
    {
        s.round.n = (int) r["n"];
        s.round.seed = (juce::int64) r["seed"];
        s.round.level = juce::jlimit (1, 10, (int) r["level"]);
        s.round.startsInMs = (int) r["startsInMs"];
        s.round.deadlineInMs = (int) r["deadlineInMs"];
        s.round.answered = (bool) r["answered"];
        s.round.opponentAnswered = (int) r["answeredCount"] > (s.round.answered ? 1 : 0);
    }

    s.place = (int) json["place"];
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
    else if (stage == Stage::ready || stage == Stage::playing)
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

    // A refused answer or accept (the round or the check closed a moment
    // earlier, or this player is out and watching) is not the battle
    // ending: keep going and let the next poll bring the state.
    if (next.errorCode.isNotEmpty() && isActive()
        && (next.errorCode == "wrong_round" || next.errorCode == "too_early"
            || next.errorCode == "no_check" || next.errorCode == "eliminated"))
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

void OnlineBattle::accept (bool yes)
{
    if (state.stage != Stage::ready)
        return;

    auto* body = new juce::DynamicObject();
    body->setProperty ("checkId", state.checkId);
    body->setProperty ("accept", yes);
    state.youAccepted = yes ? 1 : 0;

    transport ("POST", "/api/abctrain/battle/accept", juce::var (body), [flag = alive, this] (int status, const juce::var& json)
    {
        if (*flag && status != 0)
            take (status, json);
    });

    if (onChanged != nullptr)
        onChanged();
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

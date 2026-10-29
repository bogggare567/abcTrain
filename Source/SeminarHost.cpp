#include "SeminarHost.h"

SeminarHost::SeminarHost (Hooks h) : hooks (std::move (h)) {}

const std::vector<int>& SeminarHost::gamesOfFamily (int family)
{
    // GameManager order: 0 EQ, 1 Compression, 2 Reverb, 3 Pan, 4 Delay,
    // 5 Distortion, 6 Stereo width, 7 dB, 8 Frequency range.
    static const std::vector<int> families[] { { 0, 8 }, { 1, 7 }, { 2, 3, 4, 6 }, { 5 } };
    return families[juce::jlimit (0, 3, family)];
}

void SeminarHost::configure (const juce::String& title, int mask, int rounds,
                             bool listOnly, std::vector<LocalRoom::Invitee> invitees)
{
    familyMask = (mask & 0xf) != 0 ? (mask & 0xf) : 1;
    totalRounds = juce::jlimit (1, 50, rounds);
    this->title = title;
    room.configure (title, listOnly, std::move (invitees));
    makePlan();
}

void SeminarHost::makePlan()
{
    plan.clear();
    records.clear();

    if (! nextPlan.empty())
    {
        plan = std::move (nextPlan);
        nextPlan.clear();
        totalRounds = (int) plan.size();
        return;
    }

    // The chosen families in turn, an exercise of each at random - as
    // before, only decided up front so the host can see the whole list.
    std::vector<int> chosen;
    for (int f = 0; f < 4; ++f)
        if ((familyMask >> f) & 1)
            chosen.push_back (f);

    for (int r = 0; r < totalRounds; ++r)
    {
        const auto& candidates = gamesOfFamily (chosen[(size_t) (r % (int) chosen.size())]);
        plan.push_back (candidates[(size_t) random.nextInt ((int) candidates.size())]);
    }
}

bool SeminarHost::openRoom (juce::String& error)
{
    if (! room.open (8930, error))
        return false;

    stage = Stage::lobby;
    round = 0;
    return true;
}

void SeminarHost::closeRoom()
{
    setSound (false, false);
    room.close();
    stage = Stage::closed;
    round = 0;
}

void SeminarHost::start()
{
    if (stage == Stage::closed)
        return;

    round = 0;
    room.backToLobby();
    next();
}

void SeminarHost::askCurrent()
{
    if ((int) plan.size() < round)
        makePlan();

    const auto index = plan[(size_t) (round - 1)];

    if (hooks.activeGameIndex() != index)
        hooks.selectGame (index);

    // One sound for all: a fresh clip only for the first round.
    hooks.startRound (soundPerRound || round == 1);
    auto& game = hooks.game (index);

    LocalRoom::Question q;
    q.round = round;
    q.totalRounds = totalRounds;
    q.exercise = hooks.localisedName (game);
    q.prompt = hooks.localisedPrompt (game);
    q.continuous = game.usesContinuousScale();

    if (q.continuous)
    {
        for (int i = 0; i <= 100; ++i)
            q.scaleLabels.add (game.formatNormalisedValue ((float) i / 100.0f));

        for (const auto& mark : game.getGridMarks())
            if (mark.emphasised)
                q.marks.push_back ({ mark.normalised, mark.label });

        q.tolerance = game.getToleranceNormalised();
    }
    else
    {
        for (int i = 0; i < game.getNumChoices(); ++i)
            q.choices.add (game.getChoiceLabel (i));
    }

    room.ask (q);
    stage = Stage::listening;

    // The hall hears the clean version first, as in the trainer.
    setSound (true, false);
}

void SeminarHost::reveal()
{
    if (stage != Stage::listening)
        return;

    auto& game = hooks.game (hooks.activeGameIndex());
    LocalRoom::Answer a;

    if (game.usesContinuousScale())
    {
        a.value = game.getCorrectNormalised();
        a.label = game.formatNormalisedValue (a.value);
    }
    else
    {
        a.choice = game.getCorrectChoiceIndex();
        a.label = game.getChoiceLabel (a.choice);
    }

    room.reveal (a);
    stage = Stage::revealed;

    // The round as the archive keeps it: what it was, who got it wrong.
    {
        const auto snap = room.snapshot();
        SeminarArchive::Round r;
        r.game = hooks.activeGameIndex();
        r.exercise = hooks.localisedName (game);
        r.answer = a.label;
        r.sound = hooks.soundName != nullptr ? hooks.soundName() : juce::String();
        r.answered = snap.answered;
        r.right = snap.right;
        for (const auto& p : snap.players)
            if (p.voted && ! p.lastRight)
                r.missed.add (p.name);

        if ((int) records.size() >= round)
            records.resize ((size_t) round - 1);
        records.push_back (r);
    }

    // Now that everybody knows, the processed version is the lesson.
    setSound (true, true);
}

void SeminarHost::next()
{
    if (stage == Stage::closed)
        return;

    if (round >= totalRounds)
    {
        setSound (false, false);
        room.finish();
        stage = Stage::finished;

        if (onFinished != nullptr && ! records.empty())
        {
            SeminarArchive::Session s;
            s.title = title;
            s.when = juce::Time::getCurrentTime();
            s.anonymous = room.isAnonymous();
            s.people = (int) room.snapshot().players.size();
            s.rounds = records;
            onFinished (s);
        }
        return;
    }

    ++round;
    askCurrent();
}

void SeminarHost::again()
{
    setSound (false, false);
    room.backToLobby();
    round = 0;
    records.clear();
    stage = Stage::lobby;
}

void SeminarHost::setSound (bool play, bool shouldBeProcessed)
{
    playing = play;
    processed = shouldBeProcessed;

    if (hooks.setSound != nullptr)
        hooks.setSound (play, shouldBeProcessed);
}

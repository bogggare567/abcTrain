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
    room.configure (title, listOnly, std::move (invitees));
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
    // The round's family: the chosen ones in turn.
    std::vector<int> chosen;
    for (int f = 0; f < 4; ++f)
        if ((familyMask >> f) & 1)
            chosen.push_back (f);

    const auto family = chosen[(size_t) ((round - 1) % (int) chosen.size())];
    const auto& candidates = gamesOfFamily (family);
    const auto index = candidates[(size_t) random.nextInt ((int) candidates.size())];

    if (hooks.activeGameIndex() != index)
        hooks.selectGame (index);

    hooks.startRound();
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
    stage = Stage::lobby;
}

void SeminarHost::setSound (bool play, bool shouldBeProcessed)
{
    playing = play;
    processed = shouldBeProcessed;

    if (hooks.setSound != nullptr)
        hooks.setSound (play, shouldBeProcessed);
}

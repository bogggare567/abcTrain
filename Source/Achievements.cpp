#include "Achievements.h"
#include "../shared/AbcTrainTheme.h"

namespace Achievements
{
    namespace
    {
        // Game indices, matching GameManager's registration order. They are
        // append-only for the same reason ProgressManager's per-exercise
        // stats are (see decisions/021): these ids are persisted, and
        // reordering the list would silently move every player's earned
        // achievements onto different exercises.
        enum GameIndex
        {
            eq = 0, compression, reverb, pan, delay,
            distortion, stereoWidth, gain, frequencyRange
        };

        const std::vector<Definition> definitions
        {
            // Bronze: reachable in a first sitting or two. These exist so
            // the shelf is never empty, which is the only reason an easy
            // achievement is worth having.
            { "first.hundred",     Kind::totalCorrect,      100, Tier::bronze, -1,
              "achv.firstHundred.name", "achv.firstHundred.desc" },
            { "breadth.all",       Kind::breadth,             9, Tier::bronze, -1,
              "achv.breadthAll.name", "achv.breadthAll.desc" },
            { "streak.7days",      Kind::dayStreak,           7, Tier::bronze, -1,
              "achv.streak7.name", "achv.streak7.desc" },
            { "survival.25",       Kind::survivalScore,      25, Tier::bronze, -1,
              "achv.survival25.name", "achv.survival25.desc" },
            { "blitz.30",          Kind::blitzScore,         30, Tier::bronze, -1,
              "achv.blitz30.name", "achv.blitz30.desc" },

            // Silver: one exercise taken seriously.
            { "eq.rounds.200",     Kind::exerciseRounds,    200, Tier::silver, eq,
              "achv.eqRounds.name", "achv.eqRounds.desc" },
            { "eq.accuracy.75",    Kind::exerciseAccuracy,   75, Tier::silver, eq,
              "achv.eqAccuracy.name", "achv.eqAccuracy.desc" },
            { "range.accuracy.80", Kind::exerciseAccuracy,   80, Tier::silver, frequencyRange,
              "achv.rangeAccuracy.name", "achv.rangeAccuracy.desc" },
            { "comp.accuracy.75",  Kind::exerciseAccuracy,   75, Tier::silver, compression,
              "achv.compAccuracy.name", "achv.compAccuracy.desc" },
            { "gain.accuracy.80",  Kind::exerciseAccuracy,   80, Tier::silver, gain,
              "achv.gainAccuracy.name", "achv.gainAccuracy.desc" },
            { "delay.accuracy.70", Kind::exerciseAccuracy,   70, Tier::silver, delay,
              "achv.delayAccuracy.name", "achv.delayAccuracy.desc" },
            { "reverb.accuracy.75", Kind::exerciseAccuracy,  75, Tier::silver, reverb,
              "achv.reverbAccuracy.name", "achv.reverbAccuracy.desc" },
            { "width.accuracy.75", Kind::exerciseAccuracy,   75, Tier::silver, stereoWidth,
              "achv.widthAccuracy.name", "achv.widthAccuracy.desc" },
            { "dist.accuracy.75",  Kind::exerciseAccuracy,   75, Tier::silver, distortion,
              "achv.distAccuracy.name", "achv.distAccuracy.desc" },
            { "pan.streak.15",     Kind::answerStreak,       15, Tier::silver, pan,
              "achv.panStreak.name", "achv.panStreak.desc" },

            // Gold: months, not weeks.
            //
            // Level achievements are back. They were deliberately left out
            // when level was one global number you could pick from a
            // dropdown - an achievement earned by opening a menu. Levels
            // are now per exercise and have to be earned through a
            // promotion test, so "level 5 in this exercise" is once again a
            // real claim about the player. See decisions/025.
            { "eq.level.5",        Kind::exerciseLevel,       5, Tier::gold, eq,
              "achv.eqLevel5.name", "achv.eqLevel5.desc" },
            { "comp.level.5",      Kind::exerciseLevel,       5, Tier::gold, compression,
              "achv.compLevel5.name", "achv.compLevel5.desc" },
            { "reverb.level.5",    Kind::exerciseLevel,       5, Tier::gold, reverb,
              "achv.reverbLevel5.name", "achv.reverbLevel5.desc" },
            { "streak.30days",     Kind::dayStreak,          30, Tier::gold, -1,
              "achv.streak30.name", "achv.streak30.desc" },
            { "total.1000",        Kind::totalCorrect,     1000, Tier::gold, -1,
              "achv.total1000.name", "achv.total1000.desc" },
            { "every.level.3",     Kind::everyExerciseLevel,  3, Tier::gold, -1,
              "achv.everyLevel3.name", "achv.everyLevel3.desc" },

            // Legendary: three of them, and they are supposed to look
            // impossible from where a new player is standing. A shelf
            // where everything is collectable in a month is a shelf nobody
            // looks at twice.
            { "every.level.10",    Kind::everyExerciseLevel, 10, Tier::platinum, -1,
              "achv.everyLevel10.name", "achv.everyLevel10.desc" },
            { "streak.365days",    Kind::dayStreak,         365, Tier::platinum, -1,
              "achv.streak365.name", "achv.streak365.desc" },
            { "eq.accuracy.92",    Kind::exerciseAccuracy,   92, Tier::platinum, eq,
              "achv.eqAccuracy92.name", "achv.eqAccuracy92.desc" }
        };

        const Snapshot::PerGame* gameAt (const Snapshot& snapshot, int index)
        {
            if (index < 0 || index >= (int) snapshot.games.size())
                return nullptr;

            return &snapshot.games[(size_t) index];
        }

        // 0..1, clamped. Split out so isEarned and progressTowards can
        // never disagree about what "done" means - they are the same
        // computation, one of them thresholded.
        float rawProgress (const Definition& definition, const Snapshot& snapshot)
        {
            const auto ratio = [] (double have, double need)
            {
                return need <= 0.0 ? 1.0f
                                   : (float) juce::jlimit (0.0, 1.0, have / need);
            };

            switch (definition.kind)
            {
                case Kind::totalCorrect:
                {
                    auto total = 0;
                    for (const auto& game : snapshot.games)
                        total += game.correctAnswers;

                    return ratio (total, definition.threshold);
                }

                case Kind::breadth:
                {
                    auto played = 0;
                    for (const auto& game : snapshot.games)
                        if (game.roundsPlayed > 0)
                            ++played;

                    return ratio (played, definition.threshold);
                }

                case Kind::exerciseRounds:
                    if (auto* game = gameAt (snapshot, definition.gameIndex))
                        return ratio (game->roundsPlayed, definition.threshold);
                    return 0.0f;

                case Kind::exerciseAccuracy:
                {
                    auto* game = gameAt (snapshot, definition.gameIndex);

                    if (game == nullptr || game->roundsPlayed <= 0)
                        return 0.0f;

                    // Two gates - enough rounds, and enough of them right -
                    // so show whichever is further behind. Reporting only
                    // the accuracy would show "90% of the way there" to
                    // someone three rounds in who cannot possibly earn it.
                    const auto accuracy = 100.0 * (double) game->correctAnswers
                                                 / (double) game->roundsPlayed;

                    return juce::jmin (ratio (game->roundsPlayed, accuracyMinimumRounds),
                                       ratio (accuracy, definition.threshold));
                }

                case Kind::answerStreak:
                    if (auto* game = gameAt (snapshot, definition.gameIndex))
                        return ratio (game->bestStreak, definition.threshold);
                    return 0.0f;

                case Kind::survivalScore:
                {
                    auto best = 0;
                    for (const auto& game : snapshot.games)
                        best = juce::jmax (best, game.bestSurvivalScore);

                    return ratio (best, definition.threshold);
                }

                case Kind::blitzScore:
                {
                    auto best = 0;
                    for (const auto& game : snapshot.games)
                        best = juce::jmax (best, game.bestBlitzScore);

                    return ratio (best, definition.threshold);
                }

                case Kind::dayStreak:
                    return ratio (snapshot.streakDays, definition.threshold);

                case Kind::exerciseLevel:
                    if (auto* game = gameAt (snapshot, definition.gameIndex))
                        return ratio (game->level, definition.threshold);
                    return 0.0f;

                case Kind::everyExerciseLevel:
                {
                    if (snapshot.games.empty())
                        return 0.0f;

                    // The *weakest* exercise decides. Reporting an average
                    // would show someone with eight beginners and one
                    // expert as most of the way there, which is the
                    // opposite of what this asks.
                    auto lowest = snapshot.games.front().level;

                    for (const auto& game : snapshot.games)
                        lowest = juce::jmin (lowest, game.level);

                    return ratio (lowest, definition.threshold);
                }
            }

            return 0.0f;
        }
    }

    juce::Colour colourForTier (Tier tier) noexcept
    {
        switch (tier)
        {
            case Tier::bronze:    return juce::Colour (0xffb8763f);
            case Tier::silver:    return juce::Colour (0xffb2bac4);
            case Tier::gold:      return juce::Colour (0xffd7ac4e);
            case Tier::platinum:  return juce::Colour (0xffbfe0e6);
        }

        return juce::Colour (0xffb9c0c9);
    }

    const std::vector<Definition>& all()
    {
        return definitions;
    }

    const Definition* find (const juce::String& id)
    {
        for (const auto& definition : definitions)
            if (id == definition.id)
                return &definition;

        return nullptr;
    }

    bool isEarned (const Definition& definition, const Snapshot& snapshot)
    {
        return rawProgress (definition, snapshot) >= 1.0f;
    }

    std::vector<juce::String> evaluate (const Snapshot& snapshot)
    {
        std::vector<juce::String> earned;

        for (const auto& definition : definitions)
            if (isEarned (definition, snapshot))
                earned.push_back (definition.id);

        return earned;
    }

    float progressTowards (const Definition& definition, const Snapshot& snapshot)
    {
        return rawProgress (definition, snapshot);
    }
}

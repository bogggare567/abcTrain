#include "Achievements.h"
#include "../shared/AbcTrainTheme.h"
#include <string>

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

        // Built once. Every per-exercise rule is generated for all nine
        // exercises from one line, so a new exercise gets its stamps and its
        // milestone by being appended to GameIndex - and an id, once
        // shipped, never changes (it is the persistence key).
        std::vector<Definition> makeDefinitions()
        {
            std::vector<Definition> d;
            const char* ids[] { "eq", "comp", "reverb", "pan", "delay", "dist", "width", "gain", "range" };

            // --- milestones: what you can hear ---------------------------
            //
            // One per exercise, at step 8: the lower half of "mixing ear".
            // The staircase only holds a step when you are right ~4 times
            // in 5 there, so this is a measured threshold, not hours served.
            static const char* milestoneNames[] {
                "ms.eq.name", "ms.comp.name", "ms.reverb.name", "ms.pan.name", "ms.delay.name",
                "ms.dist.name", "ms.width.name", "ms.gain.name", "ms.range.name" };

            for (int g = 0; g < 9; ++g)
                d.push_back ({ nullptr, Kind::exerciseLevel, 8, Tier::gold, g,
                               milestoneNames[g], "ms.exercise.desc", Layer::milestone });

            d.push_back ({ "ms.all5",   Kind::everyExerciseLevel, 5, Tier::gold,     -1, "ms.all5.name",   "ms.all5.desc",   Layer::milestone });
            d.push_back ({ "ms.all9",   Kind::everyExerciseLevel, 9, Tier::platinum, -1, "ms.all9.name",   "ms.all9.desc",   Layer::milestone });
            d.push_back ({ "ms.days30", Kind::dayStreak,         30, Tier::gold,     -1, "ms.days30.name", "ms.days30.desc", Layer::milestone });

            // --- stamps: what you did -------------------------------------
            for (int g = 0; g < 9; ++g)
            {
                d.push_back ({ nullptr, Kind::exerciseRounds,  50, Tier::bronze, g, "st.rounds.name", "st.exercise.desc" });
                d.push_back ({ nullptr, Kind::exerciseRounds, 300, Tier::silver, g, "st.rounds.name", "st.exercise.desc" });
                d.push_back ({ nullptr, Kind::answerStreak,    10, Tier::silver, g, "st.streak.name", "st.exercise.desc" });
                d.push_back ({ nullptr, Kind::exerciseLevel,    4, Tier::bronze, g, "st.level.name",  "st.exercise.desc" });
            }

            for (const auto n : { 100, 500, 2000, 5000 })
                d.push_back ({ nullptr, Kind::totalCorrect, n, n >= 5000 ? Tier::platinum : n >= 2000 ? Tier::silver : Tier::bronze, -1,
                               "st.total.name", "st.total.desc" });

            for (const auto n : { 3, 7, 14, 60, 100, 365 })
                d.push_back ({ nullptr, Kind::dayStreak, n, n >= 365 ? Tier::platinum : n >= 60 ? Tier::silver : Tier::bronze, -1,
                               "st.days.name", "st.days.desc" });

            for (const auto n : { 10, 25, 50 })
                d.push_back ({ nullptr, Kind::survivalScore, n, Tier::bronze, -1, "st.survival.name", "st.run.desc" });

            for (const auto n : { 15, 30, 45 })
                d.push_back ({ nullptr, Kind::blitzScore, n, Tier::bronze, -1, "st.blitz.name", "st.run.desc" });

            d.push_back ({ nullptr, Kind::breadth, 9, Tier::bronze, -1, "st.breadth.name", "st.breadth.desc" });

            // Ids are generated from the rule itself - "st.reverb.rounds.300",
            // "ms.eq.level.8" - so they are stable, unique and readable in a
            // settings file without a separate table that could drift.
            static std::vector<std::string> idStore;
            idStore.clear();
            idStore.reserve (d.size());

            for (auto& def : d)
            {
                if (def.id != nullptr)
                    continue;

                std::string kind;
                switch (def.kind)
                {
                    case Kind::exerciseRounds:     kind = "rounds"; break;
                    case Kind::answerStreak:       kind = "streak"; break;
                    case Kind::exerciseLevel:      kind = "level";  break;
                    case Kind::totalCorrect:       kind = "total";  break;
                    case Kind::dayStreak:          kind = "days";   break;
                    case Kind::survivalScore:      kind = "survival"; break;
                    case Kind::blitzScore:         kind = "blitz";  break;
                    case Kind::breadth:            kind = "breadth"; break;
                    case Kind::everyExerciseLevel: kind = "all";    break;
                    case Kind::exerciseAccuracy:   kind = "acc";    break;
                }

                const auto prefix = def.layer == Layer::milestone ? std::string ("ms.") : std::string ("st.");
                const auto who = def.gameIndex >= 0 ? std::string (ids[def.gameIndex]) + "." : std::string();
                idStore.push_back (prefix + who + kind + "." + std::to_string (def.threshold));
            }

            size_t next = 0;
            for (auto& def : d)
                if (def.id == nullptr)
                    def.id = idStore[next++].c_str();

            return d;
        }

        const std::vector<Definition> definitions = makeDefinitions();

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
        // Metals, picked to read on the dark page: pale silver and paler
        // platinum are what make those tiers *look* like the higher ones.
        auto colour = juce::Colour (0xffb9c0c9);

        switch (tier)
        {
            case Tier::bronze:    colour = juce::Colour (0xffb8763f); break;
            case Tier::silver:    colour = juce::Colour (0xffb2bac4); break;
            case Tier::gold:      colour = juce::Colour (0xffd7ac4e); break;
            case Tier::platinum:  colour = juce::Colour (0xffbfe0e6); break;
        }

        // On the light page those same two are near-invisible as text on an
        // off-white card - caught by looking at the light snapshot, which is
        // the recurring bug class this project keeps re-learning. Darkening
        // is proportional to how pale the metal is, so bronze and gold are
        // barely touched and silver/platinum come down far enough to read
        // while still ranking above bronze by brightness.
        if (AbcTrainTheme::getMode() == AbcTrainTheme::Mode::light)
            colour = colour.darker (colour.getPerceivedBrightness() * 0.95f);

        return colour;
    }

    const char* nameKeyForTier (Tier tier) noexcept
    {
        switch (tier)
        {
            case Tier::bronze:   return "tier.bronze";
            case Tier::silver:   return "tier.silver";
            case Tier::gold:     return "tier.gold";
            case Tier::platinum: return "tier.platinum";
        }

        return "tier.bronze";
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

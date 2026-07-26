#include "Achievements.h"

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
            // --- getting started, one per skill family -------------------
            { "first.hundred", Kind::totalCorrect, 100, -1,
              "achv.firstHundred.name", "achv.firstHundred.desc" },
            { "breadth.all", Kind::breadth, 9, -1,
              "achv.breadthAll.name", "achv.breadthAll.desc" },

            // --- frequency ------------------------------------------------
            { "eq.rounds.200", Kind::exerciseRounds, 200, eq,
              "achv.eqRounds.name", "achv.eqRounds.desc" },
            { "eq.accuracy.75", Kind::exerciseAccuracy, 75, eq,
              "achv.eqAccuracy.name", "achv.eqAccuracy.desc" },
            { "range.accuracy.80", Kind::exerciseAccuracy, 80, frequencyRange,
              "achv.rangeAccuracy.name", "achv.rangeAccuracy.desc" },

            // --- dynamics -------------------------------------------------
            { "comp.accuracy.75", Kind::exerciseAccuracy, 75, compression,
              "achv.compAccuracy.name", "achv.compAccuracy.desc" },
            { "gain.accuracy.80", Kind::exerciseAccuracy, 80, gain,
              "achv.gainAccuracy.name", "achv.gainAccuracy.desc" },

            // --- space and stereo -----------------------------------------
            { "pan.streak.15", Kind::answerStreak, 15, pan,
              "achv.panStreak.name", "achv.panStreak.desc" },
            { "delay.accuracy.70", Kind::exerciseAccuracy, 70, delay,
              "achv.delayAccuracy.name", "achv.delayAccuracy.desc" },
            { "reverb.accuracy.75", Kind::exerciseAccuracy, 75, reverb,
              "achv.reverbAccuracy.name", "achv.reverbAccuracy.desc" },
            { "width.accuracy.75", Kind::exerciseAccuracy, 75, stereoWidth,
              "achv.widthAccuracy.name", "achv.widthAccuracy.desc" },

            // --- character ------------------------------------------------
            { "dist.accuracy.75", Kind::exerciseAccuracy, 75, distortion,
              "achv.distAccuracy.name", "achv.distAccuracy.desc" },

            // --- how you play ---------------------------------------------
            { "survival.25", Kind::survivalScore, 25, -1,
              "achv.survival25.name", "achv.survival25.desc" },
            { "blitz.30", Kind::blitzScore, 30, -1,
              "achv.blitz30.name", "achv.blitz30.desc" },
            { "streak.7days", Kind::dayStreak, 7, -1,
              "achv.streak7.name", "achv.streak7.desc" },
            { "streak.30days", Kind::dayStreak, 30, -1,
              "achv.streak30.name", "achv.streak30.desc" },
            { "total.1000", Kind::totalCorrect, 1000, -1,
              "achv.total1000.name", "achv.total1000.desc" }

            // No "reached level N" achievement, deliberately, even though
            // Kind::levelReached exists for it. Level is player-selectable
            // from a dropdown (ProgressManager::setLevelManually, added so
            // difficulty isn't only an automatic side effect of points),
            // and one notion of level is deliberately shared by the earned
            // and chosen paths - so an achievement for reaching one would
            // be earned by opening a menu. That is the exact shape of the
            // participation trophy this list is trying not to have.
            //
            // The kind stays because it costs nothing and a future
            // *earned-only* level counter would use it.
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

                case Kind::levelReached:
                    return ratio (snapshot.level, definition.threshold);
            }

            return 0.0f;
        }
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

#include <juce_core/juce_core.h>
#include "../Source/Achievements.h"

// Achievements are pure functions over a snapshot, so this needs no
// PropertiesFile, no message loop and no GameManager - the same seam
// SessionManagerTest uses (see docs/testing-strategy.md).
class AchievementsTest : public juce::UnitTest
{
public:
    AchievementsTest() : juce::UnitTest ("Achievements") {}

    void runTest() override
    {
        beginTest ("every definition has a stable, unique id");
        {
            juce::StringArray ids;

            for (const auto& definition : Achievements::all())
            {
                const juce::String id (definition.id);
                expect (id.isNotEmpty(), "an achievement has an empty id");
                expect (! ids.contains (id), "duplicate achievement id: " + id);
                ids.add (id);

                // Both keys are looked up through LocalisationManager, which
                // falls back to the key itself - so a missing one shows as
                // "achv.foo.name" on screen rather than as a crash. Still
                // worth catching an empty one here.
                expect (juce::String (definition.nameKey).isNotEmpty());
                expect (juce::String (definition.descriptionKey).isNotEmpty());
            }

            expect (ids.size() >= 10, "the list looks suspiciously short");
        }

        beginTest ("find returns null for an unknown id rather than asserting");
        {
            expect (Achievements::find ("no.such.achievement") == nullptr);
            expect (Achievements::find (Achievements::all().front().id) != nullptr);
        }

        beginTest ("a blank slate has earned nothing");
        {
            Achievements::Snapshot snapshot;
            snapshot.games.resize (9);

            expect (Achievements::evaluate (snapshot).empty());

            for (const auto& definition : Achievements::all())
                expectWithinAbsoluteError (Achievements::progressTowards (definition, snapshot),
                                            0.0f, 0.0001f);
        }

        beginTest ("accuracy needs a floor of rounds, not just a good ratio");
        {
            const auto* definition = Achievements::find ("eq.accuracy.75");
            expect (definition != nullptr);

            // Three for three is 100% accuracy and must not earn anything:
            // without the floor, the hardest-sounding achievements would be
            // the first ones anyone got.
            Achievements::Snapshot lucky;
            lucky.games.resize (9);
            lucky.games[0].roundsPlayed = 3;
            lucky.games[0].correctAnswers = 3;

            expect (! Achievements::isEarned (*definition, lucky));
            expect (Achievements::progressTowards (*definition, lucky) < 1.0f);

            // The same ratio over enough rounds does earn it.
            Achievements::Snapshot earned;
            earned.games.resize (9);
            earned.games[0].roundsPlayed = Achievements::accuracyMinimumRounds;
            earned.games[0].correctAnswers = Achievements::accuracyMinimumRounds;

            expect (Achievements::isEarned (*definition, earned));
        }

        beginTest ("accuracy just under the threshold is not earned");
        {
            const auto* definition = Achievements::find ("eq.accuracy.75");
            expect (definition != nullptr);

            Achievements::Snapshot snapshot;
            snapshot.games.resize (9);
            snapshot.games[0].roundsPlayed = 100;
            snapshot.games[0].correctAnswers = 74;
            expect (! Achievements::isEarned (*definition, snapshot));

            snapshot.games[0].correctAnswers = 75;
            expect (Achievements::isEarned (*definition, snapshot));
        }

        beginTest ("totals add up across exercises, streaks do not");
        {
            const auto* total = Achievements::find ("first.hundred");
            const auto* streak = Achievements::find ("pan.streak.15");
            expect (total != nullptr && streak != nullptr);

            Achievements::Snapshot snapshot;
            snapshot.games.resize (9);

            // 50 + 50 across two exercises is 100 correct answers.
            snapshot.games[0].roundsPlayed = 50;
            snapshot.games[0].correctAnswers = 50;
            snapshot.games[1].roundsPlayed = 50;
            snapshot.games[1].correctAnswers = 50;
            expect (Achievements::isEarned (*total, snapshot));

            // A best streak is per-exercise and does not accumulate: 8 in
            // one and 8 in another is not a streak of 16.
            snapshot.games[3].bestStreak = 8;
            snapshot.games[4].bestStreak = 8;
            expect (! Achievements::isEarned (*streak, snapshot));

            snapshot.games[3].bestStreak = 15;
            expect (Achievements::isEarned (*streak, snapshot));
        }

        beginTest ("breadth counts exercises touched, not rounds played");
        {
            const auto* definition = Achievements::find ("breadth.all");
            expect (definition != nullptr);

            Achievements::Snapshot lopsided;
            lopsided.games.resize (9);
            lopsided.games[0].roundsPlayed = 5000;
            expect (! Achievements::isEarned (*definition, lopsided));
            expectWithinAbsoluteError (Achievements::progressTowards (*definition, lopsided),
                                        1.0f / 9.0f, 0.001f);

            Achievements::Snapshot broad;
            broad.games.resize (9);
            for (auto& game : broad.games)
                game.roundsPlayed = 1;

            expect (Achievements::isEarned (*definition, broad));
        }

        beginTest ("survival and blitz take the best run on any exercise");
        {
            const auto* survival = Achievements::find ("survival.25");
            expect (survival != nullptr);

            Achievements::Snapshot snapshot;
            snapshot.games.resize (9);
            snapshot.games[6].bestSurvivalScore = 25;
            expect (Achievements::isEarned (*survival, snapshot));
        }

        beginTest ("progress is monotonic and clamped to 0..1");
        {
            const auto* definition = Achievements::find ("streak.7days");
            expect (definition != nullptr);

            Achievements::Snapshot snapshot;
            snapshot.games.resize (9);

            auto previous = -1.0f;

            for (auto days = 0; days <= 40; ++days)
            {
                snapshot.streakDays = days;
                const auto progress = Achievements::progressTowards (*definition, snapshot);

                expect (progress >= 0.0f && progress <= 1.0f, "progress left 0..1");
                expect (progress >= previous, "progress went backwards");
                previous = progress;
            }

            // Far past the threshold still reads as done, not as overflow.
            expectWithinAbsoluteError (previous, 1.0f, 0.0001f);
        }

        beginTest ("a snapshot with no games at all is handled, not crashed");
        {
            // A guard that matters: makeAchievementSnapshot sizes its
            // vector from GameManager, and a per-exercise rule indexes into
            // it. An empty one must miss rather than read past the end.
            Achievements::Snapshot empty;

            expect (Achievements::evaluate (empty).empty());

            for (const auto& definition : Achievements::all())
                expect (! Achievements::isEarned (definition, empty));
        }

        beginTest ("no achievement is earned by choosing a level");
        {
            // Level is player-selectable from a dropdown, so an achievement
            // for reaching one would be earned by opening a menu. See the
            // note in Achievements.cpp.
            Achievements::Snapshot topLevel;
            topLevel.games.resize (9);
            topLevel.level = 10;

            expect (Achievements::evaluate (topLevel).empty());
        }
    }
};

static AchievementsTest achievementsTest;

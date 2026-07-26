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

            // Not zero progress on everything, though: everyone starts every
            // exercise at level 1, so a "reach level 5" rule is genuinely
            // one fifth of the way there before a single answer. What must
            // hold is that nothing is *finished*.
            for (const auto& definition : Achievements::all())
                expect (Achievements::progressTowards (definition, snapshot) < 1.0f,
                         juce::String ("earned on a blank slate: ") + definition.id);
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

        beginTest ("an every-exercise rule is decided by the weakest exercise");
        {
            const auto* definition = Achievements::find ("every.level.3");
            expect (definition != nullptr);

            // Eight experts and one beginner is not "most of the way
            // there" - reporting an average would say it was.
            Achievements::Snapshot lopsided;
            lopsided.games.resize (9);
            for (auto& game : lopsided.games)
                game.level = 10;
            lopsided.games[4].level = 1;

            expect (! Achievements::isEarned (*definition, lopsided));
            expectWithinAbsoluteError (Achievements::progressTowards (*definition, lopsided),
                                        1.0f / 3.0f, 0.001f);

            for (auto& game : lopsided.games)
                game.level = 3;

            expect (Achievements::isEarned (*definition, lopsided));
        }

        beginTest ("level achievements exist again, now that levels are earned");
        {
            // They were deliberately absent while level was one global
            // number picked from a dropdown - see decisions/024 and 025.
            // Levels are now per exercise and gated behind a promotion
            // test, so they are a real claim again.
            auto foundLevelRule = false;

            for (const auto& definition : Achievements::all())
                if (definition.kind == Achievements::Kind::exerciseLevel
                    || definition.kind == Achievements::Kind::everyExerciseLevel)
                    foundLevelRule = true;

            expect (foundLevelRule);
        }

        beginTest ("there are a few platinum ones, and they are out of reach early");
        {
            auto platinumCount = 0;

            Achievements::Snapshot busyFirstWeek;
            busyFirstWeek.games.resize (9);
            busyFirstWeek.streakDays = 7;

            for (auto& game : busyFirstWeek.games)
            {
                game.roundsPlayed = 120;
                game.correctAnswers = 100;
                game.bestStreak = 12;
                game.level = 2;
            }

            for (const auto& definition : Achievements::all())
                if (definition.tier == Achievements::Tier::platinum)
                {
                    ++platinumCount;
                    expect (! Achievements::isEarned (definition, busyFirstWeek),
                             juce::String ("a platinum achievement was earned in a first week: ")
                                 + definition.id);
                }

            expect (platinumCount >= 3, "there should be a few genuinely hard ones");
        }

    }
};

static AchievementsTest achievementsTest;

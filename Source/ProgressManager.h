#pragma once

#include "GameManager.h"
#include <juce_data_structures/juce_data_structures.h>
#include "Achievements.h"
#include <functional>
#include <memory>
#include <vector>

// Cross-session progression: **a level per exercise** (1-10, each driving
// that game's own difficulty), a daily practice streak, and one daily
// challenge.
//
// How a level moves (ADR 035): a **staircase**, the standard adaptive
// method of psychoacoustics (Levitt 1971, "transformed up-down"). Three
// correct in a row take one step harder; one wrong answer takes one step
// easier. A 3-down/1-up staircase settles where the listener is right about
// 79% of the time - which is the neighbourhood where practice teaches
// fastest and where a round feels on the edge rather than easy or hopeless.
//
// It replaced points plus a five-in-a-row promotion test. That model hit
// the ceiling after ~360 correct answers per exercise (about an hour) and
// then said nothing new about the player for the rest of their life; and a
// level that only ever went up measured time served, not hearing. The
// staircase measures: the level it hovers at *is* the player's threshold,
// and each exercise reports it in its own units (Game::describeLevel).
//
// Two numbers per exercise, because the staircase moves both ways:
// `level` is where it is now (today's form, can dip), `bestLevel` is the
// highest step ever held (the record, never drops). Screens lead with the
// record, so a wrong answer never reads as a loss.
class ProgressManager : public juce::ChangeBroadcaster,
                         private juce::ChangeListener
{
public:
    // Uses the real user application-data location.
    explicit ProgressManager (GameManager& gameManagerToTrack);

    // Lets tests point persistence at a temp file instead of real user
    // app-data.
    ProgressManager (GameManager& gameManagerToTrack, const juce::PropertiesFile::Options& options);

    ~ProgressManager() override;

    // ---- per-exercise level ---------------------------------------------
    //
    // Everything here takes a game index. Out-of-range returns a harmless
    // default rather than asserting, the same rule getStatsForGame follows.
    int getLevelForGame (int gameIndex) const noexcept;       // current step
    int getBestLevelForGame (int gameIndex) const noexcept;   // record, never drops

    // Correct answers in a row toward the next step, 0..stepUpAfter-1, and
    // the same as a 0..1 fraction. The three squares beside the exercise
    // name are this number.
    int getStepRunForGame (int gameIndex) const noexcept;
    float getLevelProgressForGame (int gameIndex) const noexcept;

    // ---- the measured threshold (Levitt 1971) ---------------------------
    //
    // The record above is the highest step the staircase ever touched, and
    // the highest point of a random walk is systematically optimistic:
    // three lucky answers on a wide band happen, and the record keeps them
    // for ever. Psychoacoustics does not read a threshold off the top of a
    // staircase; it averages the *reversals* - the steps where it turned
    // round - and so does this: the mean of the last six to eight, once
    // there are six. That is the number a teacher can compare before and
    // after, and the one the home screen shows once it exists.
    //
    // A fractional step (6.5 = between steps 6 and 7). Negative when there
    // are not enough reversals yet.
    float getThresholdLevelForGame (int gameIndex) const noexcept;
    bool hasThresholdForGame (int gameIndex) const noexcept { return getThresholdLevelForGame (gameIndex) > 0.0f; }
    static constexpr int reversalsForThreshold = 6;
    static constexpr int reversalsKept = 8;

    // How often each part of the subject should come up next, from where
    // the misses land (Kaniwa et al. 2011: weight follows the error rate,
    // with a floor so a strong part is still asked). A bucket with fewer
    // than three attempts gets the neutral weight, so an untried part of the
    // subject is not starved. Pushed into each Game after every answer.
    std::vector<float> computeBucketWeights (int gameIndex) const;

    // The highest record across all exercises.
    int getMaxLevelReached() const noexcept;

    int getStreakDays() const noexcept { return streakDays; }

    // Seconds actually spent inside an exercise, with the signal running.
    //
    // Not time with the window open. A plugin left loaded in a project for
    // a working day has not been used for a working day, and asking it for
    // anything on that basis would be a lie about what the person got.
    void addPracticeSecond();
    int getPracticeSeconds() const noexcept { return practiceSeconds; }

    // How many correct in a row on this exercise right now. Already
    // tracked (it is what completes the daily challenge); exposing it is
    // what lets the challenge be shown as progress toward something
    // rather than as a binary that flips at the end. Out-of-range
    // returns 0, the same graceful-miss rule as getStatsForGame.
    int getConsecutiveCorrectForGame (int gameIndex) const noexcept;

    // Deliberately *not* a formatted sentence. It used to return English
    // prose built right here - "Daily challenge: get 5 correct in a row on
    // \"Guess the Distortion\"" - which then appeared verbatim inside a
    // Russian UI, complete with the English exercise name. This class has
    // no LocalisationManager and shouldn't: it reports the numbers, and
    // the editor (which does know the language, and knows how to translate
    // an exercise name) writes the sentence.
    int getDailyChallengeTargetStreak() const noexcept { return dailyChallengeTargetStreak; }
    bool isDailyChallengeComplete() const noexcept { return dailyChallengeComplete; }
    int getDailyChallengeGameIndex() const noexcept { return dailyChallengeGameIndex; }

    // Pure, no side effects - exposed so tests can check the date math
    // directly without constructing a whole ProgressManager.
    static int daysBetween (const juce::String& isoDateA, const juce::String& isoDateB);

    // Same logic the constructor runs against the real current date, but
    // callable directly with an explicit date so tests don't need to mock
    // the system clock.
    void updateStreakForDate (const juce::String& todayIso);
    void generateDailyChallengeForDate (const juce::String& todayIso);

    // Applies one answer's effect directly - scoring, consecutive-correct
    // tracking, daily challenge check. changeListenerCallback below is a
    // thin wrapper around this. Exposed directly because
    // juce::ChangeBroadcaster::sendChangeMessage() is asynchronous (needs
    // a running JUCE message loop to actually deliver), which
    // EarTrainerTests - a plain console app - never pumps; testing
    // through the real listener chain would be unreliable at best and
    // could hang the test binary at worst. This is the seam tests use
    // instead.
    // `quality` is 0..1 from Game::getAnswerQuality. It no longer affects
    // progress - there are no points to scale - and is kept so callers and
    // tests keep one signature.
    void registerAnswer (int gameIndex, bool wasCorrect, float quality = 1.0f,
                          int skillBucket = -1);

    // Where this exercise's misses land. See Game::getNumSkillBuckets -
    // the buckets are the exercise's own division of its subject, so
    // "you keep missing in the low-mids" and "you keep missing Plate" come
    // out of the same two counters.
    static constexpr int maxSkillBuckets = 8;

    int getBucketAttempts (int gameIndex, int bucket) const;
    int getBucketMisses (int gameIndex, int bucket) const;

    // Lifetime per-exercise record, persisted alongside the level.
    // Kept separate from each Game's own getScore()/getRoundsPlayed(),
    // which are deliberately in-memory session counters that reset every
    // time the plugin is reopened - this is the "how am I doing at this
    // exercise, ever" number the training picker shows on each card.
    struct GameStats
    {
        int roundsPlayed = 0;
        int correctAnswers = 0;
        int bestStreak = 0;
        int bestSurvivalScore = 0;
        int bestBlitzScore = 0;

        float getAccuracy() const noexcept
        {
            return roundsPlayed > 0 ? (float) correctAnswers / (float) roundsPlayed : 0.0f;
        }
    };

    GameStats getStatsForGame (int gameIndex) const;

    // Which mode this exercise was left in, and setting it.
    //
    // Per exercise, because the mode is a property of how you are working
    // on *that* skill, not a global switch. Leaving a Blitz run, going
    // home and opening something else used to drop you straight into a
    // Blitz countdown on an exercise you had only ever practised - the
    // mode followed you around instead of staying where you set it.
    int getPreferredModeForGame (int gameIndex) const;
    void setPreferredModeForGame (int gameIndex, int mode);

    // Whether Survival and Blitz are offered for this exercise yet.
    //
    // Practice is the only mode a new exercise starts with, and the timed
    // ones appear once you have shown you can actually hear the thing -
    // five right in a row. Offering all three on the first visit asks
    // somebody to pick a pressure level for a skill they have not tested,
    // and the honest answer at that point is always Practice.
    //
    // Derived from the lifetime best streak, which is already persisted,
    // rather than from a new flag: one fact, one place, and an unlock
    // that cannot disagree with the record that earned it.
    static constexpr int streakToUnlockModes = 5;

    bool areModesUnlockedForGame (int gameIndex) const
    {
        return allModesOpen || getStatsForGame (gameIndex).bestStreak >= streakToUnlockModes;
    }

    // Pro setting: every mode open from the first visit, for someone who
    // does not need to be walked in.
    void setAllModesOpen (bool shouldBeOpen) noexcept { allModesOpen = shouldBeOpen; }

    // Pro setting: how many right in a row take one step harder. 3 is the
    // default and what a beginner always gets - the 3-down/1-up rule that
    // settles near 79% correct (ADR 035). 2 settles near 71% (faster,
    // rougher), 4 near 84% (slower, more certain). Clamped to 2..4; the
    // current run toward a step is trimmed if it no longer fits.
    void setStepUpAfter (int answersInARow);
    int getStepUpAfter() const noexcept { return stepRule; }

    // "Trainings I'm interested in" - the player's own shortlist, pinned
    // to the top of the home screen. Persisted like everything else here,
    // because a focus you have to re-pick every launch isn't a focus.
    bool isFavouriteGame (int gameIndex) const;
    void setFavouriteGame (int gameIndex, bool shouldBeFavourite);
    bool hasAnyFavourites() const;

    // Records a completed Survival/Blitz run's score if it beats the
    // stored best. Called by SessionManager when a run ends.
    // ---- achievements --------------------------------------------------
    //
    // The list itself and the rules live in Achievements.{h,cpp}, which is
    // pure and testable. This class owns only two things about them: the
    // set that has been earned (persisted) and *when* to re-check.
    //
    // Re-checking happens after any state change that could earn one, and
    // is idempotent - an achievement already in the set is never announced
    // twice, which is what stops "you earned X" firing on every answer
    // after the first.
    bool hasAchievement (const juce::String& id) const;
    int getNumAchievementsEarned() const noexcept { return (int) earnedAchievements.size(); }

    // The snapshot the rules are evaluated against - exposed so the UI can
    // show how far along the *unearned* ones are without this class having
    // to mirror every rule.
    Achievements::Snapshot makeAchievementSnapshot() const;

    // Called with each id the moment it is first earned. The editor uses
    // it to show a toast; nothing else in this class depends on it, and a
    // null callback is fine.
    std::function<void (const juce::String&)> onAchievementEarned;

    // What one answer actually produced - the facts the UI needs to make
    // the moment land.
    struct AnswerOutcome
    {
        bool wasCorrect = false;
        bool dailyChallengeJustCompleted = false;
        bool leveledUp = false;      // took a step harder
        bool steppedDown = false;    // took a step easier
        bool newBest = false;        // the record moved - the moment worth marking
        int level = 1;               // after this answer
        int bestLevel = 1;
        int stepRun = 0;             // 0..stepUpAfter-1 after this answer
    };

    // Fired synchronously from registerAnswer, after state is saved and
    // before the generic change broadcast. Null is fine, same as
    // onAchievementEarned.
    std::function<void (int gameIndex, const AnswerOutcome&)> onAnswerScored;

    // ---- sync (Live account, ADR 045) ------------------------------------
    //
    // A summary small enough to live on the server for every player (about
    // a kilobyte): per exercise the level, the record and the run bests;
    // streak, practice time, achievements. Not the history, not the skill
    // buckets - those stay on this computer.
    juce::var makeSyncSummary() const;

    // Folds a summary from the server (another computer) into this one.
    // Records never drop: bests are the larger of the two, achievements the
    // union. The current level is taken from the other side only for an
    // exercise never played here - so a new computer starts where you are,
    // and a computer you have been using keeps today's form. Returns true
    // when anything here changed (saved and broadcast already).
    bool mergeSyncSummary (const juce::var& summary);

    // Bumped on every saved change, so a sync can tell "nothing new".
    juce::uint64 getChangeCounter() const noexcept { return changeCounter; }

    void recordSurvivalScore (int gameIndex, int score);
    void recordBlitzScore (int gameIndex, int score);

    static constexpr int maxLevel = 10;

    // Three right in a row for a step harder, one wrong for a step easier.
    static constexpr int stepUpAfter = 3;

    static constexpr int dailyChallengeTargetStreak = 5;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void loadState();
    void saveState();
    int indexOfGame (const Game& game) const noexcept;

    GameManager& gameManager;
    std::unique_ptr<juce::PropertiesFile> properties;

    int stepRule = stepUpAfter;
    bool allModesOpen = false;

    // One of these per exercise. `level` moves both ways; `bestLevel`
    // only ever grows. See the class comment for why.
    struct GameProgress
    {
        int level = 1;
        int bestLevel = 1;
        int stepRun = 0;

        // The staircase's turning points, newest last, at most
        // reversalsKept; and which way it last moved (+1 harder, -1 easier,
        // 0 not yet).
        std::vector<int> reversals;
        int lastDirection = 0;
    };

    void pushBucketWeights (int gameIndex);

    std::vector<GameProgress> progressPerGame;

    // One staircase step. Fills the outcome as it goes.
    void applyAnswerToProgress (int gameIndex, bool wasCorrect, AnswerOutcome& outcome);

    int streakDays = 0;
    juce::String lastSessionDate;

    juce::String dailyChallengeDate;
    int dailyChallengeGameIndex = 0;
    bool dailyChallengeComplete = false;

    std::vector<int> consecutiveCorrectPerGame;
    std::vector<GameStats> statsPerGame;

    // Ids, not indices: an achievement's id is stable across releases even
    // as the list is reordered or added to, which an index would not be.
    juce::StringArray earnedAchievements;
    void refreshAchievements();
    std::vector<bool> favouritePerGame;

    // How this exercise was last played. Stored as the SessionManager::Mode
    // ordinal rather than the enum, so ProgressManager keeps not depending
    // on SessionManager - the two have never known about each other and a
    // remembered preference is not a reason to introduce that.
    std::vector<int> preferredModePerGame;

    struct BucketStats { int attempts = 0; int misses = 0; };
    std::vector<std::array<BucketStats, maxSkillBuckets>> bucketsPerGame;

    int practiceSeconds = 0;
    juce::uint64 changeCounter = 0;
    int unsavedPracticeSeconds = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgressManager)
};

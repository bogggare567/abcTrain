#pragma once

#include "GameManager.h"
#include <juce_data_structures/juce_data_structures.h>
#include "Achievements.h"
#include <functional>
#include <memory>
#include <vector>

// Cross-session progression: **a level per exercise** (1-10, each driving
// that game's own difficulty), a daily login streak, and one daily
// challenge.
//
// Levels used to be one global number, plus a dropdown to set it directly.
// Both are gone. Being good at spotting a reverb tail says nothing about
// whether you can hear 400 Hz, so one number could only ever be wrong for
// eight exercises out of nine - and a level you can pick from a menu is a
// setting, not an achievement. Everyone now starts every exercise at 1 and
// earns each one separately.
//
// How a level is earned (see promoteIfDue): points from correct answers
// unlock the *chance* to move up, but the move itself needs a short test -
// promotionTestLength correct in a row. Points alone would mean grinding
// volume; a test alone would mean a lucky streak on day one. Together they
// say "you have put the hours in, now show me" - and while the test is
// live there is something concrete to chase, which is the point. Backed by juce::PropertiesFile. Games
// themselves know nothing about points/levels - this class listens to
// every game's ChangeBroadcaster and reacts to correct/incorrect answers
// from the outside, the same way the editor listens for UI refreshes.
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
    int getLevelForGame (int gameIndex) const noexcept;
    int getPointsForGame (int gameIndex) const noexcept;

    // 0..1 through the current level, for a per-exercise indicator.
    float getLevelProgressForGame (int gameIndex) const noexcept;

    // True while the points threshold is met and the promotion test is
    // live. getPromotionStreakForGame is how far into it the player is,
    // out of promotionTestLength.
    bool isPromotionPendingForGame (int gameIndex) const noexcept;
    int getPromotionStreakForGame (int gameIndex) const noexcept;

    // Summed across every exercise - the only global number left, and only
    // because achievements ask about totals.
    int getTotalScore() const noexcept;

    // The highest level reached in any exercise. Used to unlock training
    // sound categories, which are a whole-account thing rather than a
    // per-exercise one.
    int getMaxLevelReached() const noexcept;

    int getStreakDays() const noexcept { return streakDays; }

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
    int getDailyChallengeBonusPoints() const noexcept { return dailyChallengeBonusPoints; }
    bool isDailyChallengeComplete() const noexcept { return dailyChallengeComplete; }
    int getDailyChallengeGameIndex() const noexcept { return dailyChallengeGameIndex; }

    // Pure functions, no side effects - exposed so tests can check the
    // level/date math directly without constructing a whole ProgressManager.
    static int pointsRequiredForLevel (int level) noexcept;
    static int levelForScore (int score) noexcept;
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
    // `quality` is 0..1 from Game::getAnswerQuality - how close a
    // continuous answer landed inside its accept band. It scales the
    // points awarded, so scraping the edge of the band and hitting the
    // target dead on are no longer worth the same. Defaults to 1 so every
    // existing call site and every categorical game is unaffected.
    void registerAnswer (int gameIndex, bool wasCorrect, float quality = 1.0f);

    // Lifetime per-exercise record, persisted alongside points/level.
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
        return getStatsForGame (gameIndex).bestStreak >= streakToUnlockModes;
    }

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
    // the moment land. applyAnswerToProgress always *knew* these (it even
    // returned "the level changed" as its bool), but registerAnswer
    // discarded them, which is why a level-up used to happen with no
    // fanfare whatsoever: the one moment the whole points system builds
    // toward arrived as a silent label refresh.
    struct AnswerOutcome
    {
        int pointsAwarded = 0;              // base + precision + any daily bonus
        bool wasCorrect = false;
        bool dailyChallengeJustCompleted = false;
        bool promotionJustOpened = false;   // points threshold crossed, test now live
        bool promotionJustFailed = false;   // wrong answer reset a live test
        bool leveledUp = false;
        int level = 1;                      // after this answer
        int promotionStreak = 0;            // progress through a live test
        bool promotionPending = false;
    };

    // Fired synchronously from registerAnswer, after state is saved and
    // before the generic change broadcast. Null is fine, same as
    // onAchievementEarned.
    std::function<void (int gameIndex, const AnswerOutcome&)> onAnswerScored;

    void recordSurvivalScore (int gameIndex, int score);
    void recordBlitzScore (int gameIndex, int score);

    static constexpr int maxLevel = 10;
    static constexpr int pointsPerCorrectAnswer = 10;

    // Awarded on top, scaled by quality: a dead-centre answer is worth
    // 15, the edge of the band 10. Small on purpose - precision should be
    // worth chasing, not worth more than showing up.
    static constexpr int precisionBonusPoints = 5;

    // Correct answers in a row needed to actually take a level once the
    // points have unlocked it. Short on purpose: it is a checkpoint, not a
    // wall, and a long one would turn every promotion into a chore.
    static constexpr int promotionTestLength = 5;
    static constexpr int dailyChallengeBonusPoints = 50;
    static constexpr int dailyChallengeTargetStreak = 5;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void loadState();
    void saveState();
    int indexOfGame (const Game& game) const noexcept;

    GameManager& gameManager;
    std::unique_ptr<juce::PropertiesFile> properties;

    // One of these per exercise. `points` only ever grows; `level` only
    // ever grows; a failed promotion test costs the streak, never a level
    // or a point. Losing a level for a bad run would make people stop
    // playing the exercises they are worst at, which are the ones worth
    // playing.
    struct GameProgress
    {
        int points = 0;
        int level = 1;
        bool promotionPending = false;
        int promotionStreak = 0;
    };

    std::vector<GameProgress> progressPerGame;

    // Applies points, opens a promotion when the threshold is crossed, and
    // advances or resets the test. Fills the outcome as it goes.
    void applyAnswerToProgress (int gameIndex, bool wasCorrect, float quality,
                                AnswerOutcome& outcome);

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgressManager)
};

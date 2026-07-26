#pragma once

#include "GameManager.h"
#include <juce_data_structures/juce_data_structures.h>
#include "Achievements.h"
#include <functional>
#include <memory>
#include <vector>

// Cross-session progression: points, level (1-10, driving each game's
// difficulty via GameManager::setDifficultyForAllGames), a daily login
// streak, and one daily challenge. Backed by juce::PropertiesFile. Games
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

    int getTotalScore() const noexcept { return totalScore; }
    int getLevel() const noexcept { return level; }
    int getMaxLevelReached() const noexcept { return maxLevelReached; }
    int getPointsIntoCurrentLevel() const noexcept;
    int getPointsNeededForNextLevel() const noexcept;
    float getLevelProgressProportion() const noexcept;

    int getStreakDays() const noexcept { return streakDays; }

    juce::String getDailyChallengeDescription() const;
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
    void registerAnswer (int gameIndex, bool wasCorrect);

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

    void recordSurvivalScore (int gameIndex, int score);
    void recordBlitzScore (int gameIndex, int score);

    // Directly sets the level (clamped 1..maxLevel), bypassing the usual
    // points-driven progression - lets a player who wants direct control
    // over difficulty jump straight to a level instead of only ever
    // reaching it by accumulating points. Sets totalScore to that
    // level's exact point threshold (pointsRequiredForLevel), so the
    // level/points relationship stays internally consistent - level is
    // still always *derived* from totalScore (see addPoints()), never a
    // second independent source of truth. Does not lower
    // maxLevelReached if the new level is a step down.
    void setLevelManually (int newLevel);

    static constexpr int maxLevel = 10;
    static constexpr int pointsPerCorrectAnswer = 10;
    static constexpr int dailyChallengeBonusPoints = 50;
    static constexpr int dailyChallengeTargetStreak = 5;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void addPoints (int points);
    void loadState();
    void saveState();
    int indexOfGame (const Game& game) const noexcept;

    GameManager& gameManager;
    std::unique_ptr<juce::PropertiesFile> properties;

    int totalScore = 0;
    int level = 1;
    int maxLevelReached = 1;

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

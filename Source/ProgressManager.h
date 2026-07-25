#pragma once

#include "GameManager.h"
#include <juce_data_structures/juce_data_structures.h>
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgressManager)
};

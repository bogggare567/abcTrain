#include "ProgressManager.h"
#include <cmath>

namespace
{
    juce::PropertiesFile::Options makeDefaultOptions()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "EarTrainer";
        options.filenameSuffix = "settings";
        options.folderName = "EarTrainer";
        options.osxLibrarySubFolder = "Application Support";
        return options;
    }
}

ProgressManager::ProgressManager (GameManager& gm)
    : ProgressManager (gm, makeDefaultOptions())
{
}

ProgressManager::ProgressManager (GameManager& gm, const juce::PropertiesFile::Options& options)
    : gameManager (gm), properties (std::make_unique<juce::PropertiesFile> (options))
{
    for (int i = 0; i < gameManager.getNumGames(); ++i)
    {
        gameManager.getGame (i).addChangeListener (this);
        consecutiveCorrectPerGame.push_back (0);
        statsPerGame.push_back ({});
    }

    loadState();

    const auto today = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
    updateStreakForDate (today);
    generateDailyChallengeForDate (today);

    gameManager.setDifficultyForAllGames (level);
}

ProgressManager::~ProgressManager()
{
    for (int i = 0; i < gameManager.getNumGames(); ++i)
        gameManager.getGame (i).removeChangeListener (this);

    saveState();
}

void ProgressManager::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    // Safe: this is only ever registered as a listener on Game instances.
    auto* game = static_cast<Game*> (source);

    if (! game->hasAnswered())
        return; // a newRound() reset, not an answer - nothing to score

    const auto gameIndex = indexOfGame (*game);
    if (gameIndex < 0)
        return;

    registerAnswer (gameIndex, game->wasLastAnswerCorrect());
}

void ProgressManager::registerAnswer (int gameIndex, bool wasCorrect)
{
    // Both vectors are filled in lockstep by the one delegating
    // constructor, but this indexes raw memory - checking both is cheaper
    // than relying on that staying true.
    if (gameIndex < 0
        || gameIndex >= (int) consecutiveCorrectPerGame.size()
        || gameIndex >= (int) statsPerGame.size())
        return;

    auto& stats = statsPerGame[(size_t) gameIndex];
    ++stats.roundsPlayed;

    if (wasCorrect)
    {
        ++consecutiveCorrectPerGame[(size_t) gameIndex];
        ++stats.correctAnswers;
        stats.bestStreak = juce::jmax (stats.bestStreak, consecutiveCorrectPerGame[(size_t) gameIndex]);
        addPoints (pointsPerCorrectAnswer);

        if (! dailyChallengeComplete
            && gameIndex == dailyChallengeGameIndex
            && consecutiveCorrectPerGame[(size_t) gameIndex] >= dailyChallengeTargetStreak)
        {
            dailyChallengeComplete = true;
            addPoints (dailyChallengeBonusPoints);
        }
    }
    else
    {
        consecutiveCorrectPerGame[(size_t) gameIndex] = 0;
    }

    saveState();
    sendChangeMessage();
}

ProgressManager::GameStats ProgressManager::getStatsForGame (int gameIndex) const
{
    if (gameIndex < 0 || gameIndex >= (int) statsPerGame.size())
        return {};

    return statsPerGame[(size_t) gameIndex];
}

void ProgressManager::recordSurvivalScore (int gameIndex, int score)
{
    if (gameIndex < 0 || gameIndex >= (int) statsPerGame.size())
        return;

    auto& best = statsPerGame[(size_t) gameIndex].bestSurvivalScore;
    if (score <= best)
        return;

    best = score;
    saveState();
    sendChangeMessage();
}

void ProgressManager::recordBlitzScore (int gameIndex, int score)
{
    if (gameIndex < 0 || gameIndex >= (int) statsPerGame.size())
        return;

    auto& best = statsPerGame[(size_t) gameIndex].bestBlitzScore;
    if (score <= best)
        return;

    best = score;
    saveState();
    sendChangeMessage();
}

void ProgressManager::setLevelManually (int newLevel)
{
    const auto clamped = juce::jlimit (1, maxLevel, newLevel);
    if (clamped == level)
        return;

    totalScore = pointsRequiredForLevel (clamped);
    level = clamped;
    maxLevelReached = juce::jmax (maxLevelReached, level);
    gameManager.setDifficultyForAllGames (level);

    saveState();
    sendChangeMessage();
}

int ProgressManager::indexOfGame (const Game& game) const noexcept
{
    for (int i = 0; i < gameManager.getNumGames(); ++i)
        if (&gameManager.getGame (i) == &game)
            return i;

    return -1;
}

void ProgressManager::addPoints (int points)
{
    totalScore += points;

    const auto newLevel = levelForScore (totalScore);
    if (newLevel != level)
    {
        level = newLevel;
        maxLevelReached = juce::jmax (maxLevelReached, level);
        gameManager.setDifficultyForAllGames (level);
    }
}

int ProgressManager::pointsRequiredForLevel (int level) noexcept
{
    // Points needed to go from level L to L+1 is 100*L, so the cumulative
    // total to *reach* level L (starting at level 1, 0 points) is a
    // triangular scale - each level takes progressively more.
    int total = 0;
    for (int l = 1; l < level; ++l)
        total += 100 * l;
    return total;
}

int ProgressManager::levelForScore (int score) noexcept
{
    int lvl = 1;
    while (lvl < maxLevel && score >= pointsRequiredForLevel (lvl + 1))
        ++lvl;
    return lvl;
}

int ProgressManager::getPointsIntoCurrentLevel() const noexcept
{
    return totalScore - pointsRequiredForLevel (level);
}

int ProgressManager::getPointsNeededForNextLevel() const noexcept
{
    if (level >= maxLevel)
        return 0;
    return pointsRequiredForLevel (level + 1) - pointsRequiredForLevel (level);
}

float ProgressManager::getLevelProgressProportion() const noexcept
{
    const auto needed = getPointsNeededForNextLevel();
    if (needed <= 0)
        return 1.0f;
    return (float) getPointsIntoCurrentLevel() / (float) needed;
}

int ProgressManager::daysBetween (const juce::String& isoDateA, const juce::String& isoDateB)
{
    const auto a = juce::Time::fromISO8601 (isoDateA + "T00:00:00Z");
    const auto b = juce::Time::fromISO8601 (isoDateB + "T00:00:00Z");
    return (int) std::round ((b - a).inDays());
}

void ProgressManager::updateStreakForDate (const juce::String& todayIso)
{
    if (lastSessionDate.isEmpty())
    {
        streakDays = 1;
    }
    else if (lastSessionDate != todayIso)
    {
        const auto gap = daysBetween (lastSessionDate, todayIso);
        streakDays = (gap == 1) ? streakDays + 1 : 1;
    }
    // else: same day as the last recorded session - streak already counted.

    lastSessionDate = todayIso;
    saveState();
}

void ProgressManager::generateDailyChallengeForDate (const juce::String& todayIso)
{
    if (dailyChallengeDate == todayIso)
        return; // already generated for today

    dailyChallengeDate = todayIso;
    dailyChallengeComplete = false;

    // Deterministic per-day pick so the challenge doesn't change on reload.
    juce::Random dailyRandom ((juce::int64) todayIso.hashCode());
    dailyChallengeGameIndex = dailyRandom.nextInt (gameManager.getNumGames());

    for (auto& count : consecutiveCorrectPerGame)
        count = 0;

    saveState();
}

juce::String ProgressManager::getDailyChallengeDescription() const
{
    const auto& game = gameManager.getGame (dailyChallengeGameIndex);

    if (dailyChallengeComplete)
        return "Daily challenge complete: " + juce::String (dailyChallengeTargetStreak)
               + " in a row on \"" + game.getName() + "\" (+"
               + juce::String (dailyChallengeBonusPoints) + " bonus).";

    return "Daily challenge: get " + juce::String (dailyChallengeTargetStreak)
           + " correct in a row on \"" + game.getName() + "\".";
}

void ProgressManager::loadState()
{
    totalScore = properties->getIntValue ("totalScore", 0);
    level = levelForScore (totalScore);
    maxLevelReached = juce::jmax (level, properties->getIntValue ("maxLevelReached", level));
    streakDays = properties->getIntValue ("streakDays", 0);
    lastSessionDate = properties->getValue ("lastSessionDate");
    dailyChallengeDate = properties->getValue ("dailyChallengeDate");
    dailyChallengeGameIndex = juce::jlimit (0, juce::jmax (0, gameManager.getNumGames() - 1),
                                             properties->getIntValue ("dailyChallengeGameIndex", 0));
    dailyChallengeComplete = properties->getBoolValue ("dailyChallengeComplete", false);

    // Per-exercise lifetime stats. Keyed by index rather than by name so a
    // renamed game keeps its record; the trade-off is that *reordering*
    // GameManager's registration list would shuffle the stats, which is
    // why new games get appended there rather than inserted.
    for (size_t i = 0; i < statsPerGame.size(); ++i)
    {
        const auto prefix = "game" + juce::String ((int) i) + ".";
        auto& stats = statsPerGame[i];
        stats.roundsPlayed      = properties->getIntValue (prefix + "rounds", 0);
        stats.correctAnswers    = properties->getIntValue (prefix + "correct", 0);
        stats.bestStreak        = properties->getIntValue (prefix + "bestStreak", 0);
        stats.bestSurvivalScore = properties->getIntValue (prefix + "bestSurvival", 0);
        stats.bestBlitzScore    = properties->getIntValue (prefix + "bestBlitz", 0);
    }
}

void ProgressManager::saveState()
{
    properties->setValue ("totalScore", totalScore);
    properties->setValue ("maxLevelReached", maxLevelReached);
    properties->setValue ("streakDays", streakDays);
    properties->setValue ("lastSessionDate", lastSessionDate);
    properties->setValue ("dailyChallengeDate", dailyChallengeDate);
    properties->setValue ("dailyChallengeGameIndex", dailyChallengeGameIndex);
    properties->setValue ("dailyChallengeComplete", dailyChallengeComplete);

    for (size_t i = 0; i < statsPerGame.size(); ++i)
    {
        const auto prefix = "game" + juce::String ((int) i) + ".";
        const auto& stats = statsPerGame[i];
        properties->setValue (prefix + "rounds", stats.roundsPlayed);
        properties->setValue (prefix + "correct", stats.correctAnswers);
        properties->setValue (prefix + "bestStreak", stats.bestStreak);
        properties->setValue (prefix + "bestSurvival", stats.bestSurvivalScore);
        properties->setValue (prefix + "bestBlitz", stats.bestBlitzScore);
    }

    properties->saveIfNeeded();
}

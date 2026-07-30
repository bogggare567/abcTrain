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
        progressPerGame.push_back ({});
        favouritePerGame.push_back (false);
    }

    loadState();

    const auto today = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
    updateStreakForDate (today);
    generateDailyChallengeForDate (today);

    // Each exercise gets its own difficulty, not one shared number.
    for (int i = 0; i < gameManager.getNumGames(); ++i)
        gameManager.getGame (i).setDifficulty (getLevelForGame (i));
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

    registerAnswer (gameIndex, game->wasLastAnswerCorrect(), game->getAnswerQuality());
}

void ProgressManager::registerAnswer (int gameIndex, bool wasCorrect, float quality)
{
    // Both vectors are filled in lockstep by the one delegating
    // constructor, but this indexes raw memory - checking both is cheaper
    // than relying on that staying true.
    if (gameIndex < 0
        || gameIndex >= (int) consecutiveCorrectPerGame.size()
        || gameIndex >= (int) statsPerGame.size()
        || gameIndex >= (int) progressPerGame.size())
        return;

    auto& stats = statsPerGame[(size_t) gameIndex];
    ++stats.roundsPlayed;

    AnswerOutcome outcome;
    outcome.wasCorrect = wasCorrect;

    if (wasCorrect)
    {
        ++consecutiveCorrectPerGame[(size_t) gameIndex];
        ++stats.correctAnswers;
        stats.bestStreak = juce::jmax (stats.bestStreak, consecutiveCorrectPerGame[(size_t) gameIndex]);

        if (! dailyChallengeComplete
            && gameIndex == dailyChallengeGameIndex
            && consecutiveCorrectPerGame[(size_t) gameIndex] >= dailyChallengeTargetStreak)
        {
            dailyChallengeComplete = true;
            progressPerGame[(size_t) gameIndex].points += dailyChallengeBonusPoints;
            outcome.dailyChallengeJustCompleted = true;
            outcome.pointsAwarded += dailyChallengeBonusPoints;
        }
    }
    else
    {
        consecutiveCorrectPerGame[(size_t) gameIndex] = 0;
    }

    applyAnswerToProgress (gameIndex, wasCorrect, juce::jlimit (0.0f, 1.0f, quality), outcome);

    refreshAchievements();
    saveState();

    if (onAnswerScored != nullptr)
        onAnswerScored (gameIndex, outcome);

    sendChangeMessage();
}

Achievements::Snapshot ProgressManager::makeAchievementSnapshot() const
{
    Achievements::Snapshot snapshot;
    snapshot.streakDays = streakDays;
    snapshot.games.reserve (statsPerGame.size());

    for (size_t i = 0; i < statsPerGame.size(); ++i)
    {
        const auto& stats = statsPerGame[i];
        snapshot.games.push_back ({ stats.roundsPlayed, stats.correctAnswers, stats.bestStreak,
                                    stats.bestSurvivalScore, stats.bestBlitzScore,
                                    i < progressPerGame.size() ? progressPerGame[i].level : 1 });
    }

    return snapshot;
}

bool ProgressManager::hasAchievement (const juce::String& id) const
{
    return earnedAchievements.contains (id);
}

void ProgressManager::refreshAchievements()
{
    // Idempotent by construction: only ids not already in the set are
    // added, and only those are announced. Nothing is ever removed - an
    // achievement is a record of something that happened, so it must not
    // un-earn itself if a later run drags an average back down.
    for (const auto& id : Achievements::evaluate (makeAchievementSnapshot()))
    {
        if (earnedAchievements.contains (id))
            continue;

        earnedAchievements.add (id);

        if (onAchievementEarned != nullptr)
            onAchievementEarned (id);
    }
}

bool ProgressManager::isFavouriteGame (int gameIndex) const
{
    if (gameIndex < 0 || gameIndex >= (int) favouritePerGame.size())
        return false;

    return favouritePerGame[(size_t) gameIndex];
}

void ProgressManager::setFavouriteGame (int gameIndex, bool shouldBeFavourite)
{
    if (gameIndex < 0 || gameIndex >= (int) favouritePerGame.size())
        return;

    if (favouritePerGame[(size_t) gameIndex] == shouldBeFavourite)
        return;

    favouritePerGame[(size_t) gameIndex] = shouldBeFavourite;
    saveState();
    sendChangeMessage();
}

bool ProgressManager::hasAnyFavourites() const
{
    for (const auto favourite : favouritePerGame)
        if (favourite)
            return true;

    return false;
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
    refreshAchievements();
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
    refreshAchievements();
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

void ProgressManager::applyAnswerToProgress (int gameIndex, bool wasCorrect, float quality,
                                              AnswerOutcome& outcome)
{
    auto& game = progressPerGame[(size_t) gameIndex];

    const auto finish = [&]
    {
        outcome.level = game.level;
        outcome.promotionPending = game.promotionPending;
        outcome.promotionStreak = game.promotionStreak;
    };

    if (! wasCorrect)
    {
        // A wrong answer costs the promotion test, never a level or a
        // point. Demotion would make people avoid the exercises they are
        // worst at, which are exactly the ones worth doing.
        outcome.promotionJustFailed = game.promotionPending && game.promotionStreak > 0;
        game.promotionStreak = 0;
        finish();
        return;
    }

    const auto earned = pointsPerCorrectAnswer
                            + juce::roundToInt ((float) precisionBonusPoints * quality);
    game.points += earned;
    outcome.pointsAwarded += earned;

    if (game.level < maxLevel && ! game.promotionPending
        && game.points >= pointsRequiredForLevel (game.level + 1))
    {
        game.promotionPending = true;
        game.promotionStreak = 0;
        outcome.promotionJustOpened = true;
    }

    if (! game.promotionPending)
    {
        finish();
        return;
    }

    if (++game.promotionStreak < promotionTestLength)
    {
        finish();
        return;
    }

    ++game.level;
    game.promotionPending = false;
    game.promotionStreak = 0;
    outcome.leveledUp = true;

    // Only this exercise gets harder. That is the whole point.
    gameManager.getGame (gameIndex).setDifficulty (game.level);
    finish();
}

int ProgressManager::getLevelForGame (int gameIndex) const noexcept
{
    if (gameIndex < 0 || gameIndex >= (int) progressPerGame.size())
        return 1;

    return progressPerGame[(size_t) gameIndex].level;
}

int ProgressManager::getPointsForGame (int gameIndex) const noexcept
{
    if (gameIndex < 0 || gameIndex >= (int) progressPerGame.size())
        return 0;

    return progressPerGame[(size_t) gameIndex].points;
}

bool ProgressManager::isPromotionPendingForGame (int gameIndex) const noexcept
{
    if (gameIndex < 0 || gameIndex >= (int) progressPerGame.size())
        return false;

    return progressPerGame[(size_t) gameIndex].promotionPending;
}

int ProgressManager::getPromotionStreakForGame (int gameIndex) const noexcept
{
    if (gameIndex < 0 || gameIndex >= (int) progressPerGame.size())
        return 0;

    return progressPerGame[(size_t) gameIndex].promotionStreak;
}

float ProgressManager::getLevelProgressForGame (int gameIndex) const noexcept
{
    if (gameIndex < 0 || gameIndex >= (int) progressPerGame.size())
        return 0.0f;

    const auto& game = progressPerGame[(size_t) gameIndex];

    // Once the test is live the bar is full and the *test* is the thing to
    // watch - reporting 103% of a level nobody has taken yet would be
    // meaningless.
    if (game.promotionPending || game.level >= maxLevel)
        return 1.0f;

    const auto floorPoints = pointsRequiredForLevel (game.level);
    const auto needed = pointsRequiredForLevel (game.level + 1) - floorPoints;

    if (needed <= 0)
        return 1.0f;

    return juce::jlimit (0.0f, 1.0f, (float) (game.points - floorPoints) / (float) needed);
}

int ProgressManager::getTotalScore() const noexcept
{
    auto total = 0;
    for (const auto& game : progressPerGame)
        total += game.points;

    return total;
}

int ProgressManager::getMaxLevelReached() const noexcept
{
    auto highest = 1;
    for (const auto& game : progressPerGame)
        highest = juce::jmax (highest, game.level);

    return highest;
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
    refreshAchievements();
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

void ProgressManager::loadState()
{
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

        // Per-exercise level and points. A save file written before levels
        // were per-exercise simply has none of these keys, so everyone
        // starts every exercise at 1 - which is what was asked for, and
        // beats trying to split one global number nine ways.
        auto& gameProgress = progressPerGame[i];
        gameProgress.points          = properties->getIntValue (prefix + "points", 0);
        gameProgress.level           = juce::jlimit (1, maxLevel, properties->getIntValue (prefix + "level", 1));
        gameProgress.promotionPending = properties->getBoolValue (prefix + "promotionPending", false);
        gameProgress.promotionStreak  = juce::jmax (0, properties->getIntValue (prefix + "promotionStreak", 0));

        if (i < favouritePerGame.size())
            favouritePerGame[i] = properties->getBoolValue (prefix + "favourite", false);
    }

    earnedAchievements.clear();
    earnedAchievements.addTokens (properties->getValue ("achievements"), ",", "");
    earnedAchievements.removeEmptyStrings();

    // Anything the saved state already qualifies for but predates - a
    // player who had 500 correct answers before achievements existed
    // should not have to earn "your first hundred" again. Deliberately
    // silent: onAchievementEarned isn't set yet at load time, so this
    // backfills without a burst of toasts on first launch.
    refreshAchievements();
}

void ProgressManager::saveState()
{
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

        const auto& gameProgress = progressPerGame[i];
        properties->setValue (prefix + "points", gameProgress.points);
        properties->setValue (prefix + "level", gameProgress.level);
        properties->setValue (prefix + "promotionPending", gameProgress.promotionPending);
        properties->setValue (prefix + "promotionStreak", gameProgress.promotionStreak);

        if (i < favouritePerGame.size())
            properties->setValue (prefix + "favourite", (bool) favouritePerGame[i]);
    }

    properties->setValue ("achievements", earnedAchievements.joinIntoString (","));

    properties->saveIfNeeded();
}

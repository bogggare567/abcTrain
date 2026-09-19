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
        preferredModePerGame.push_back (0);   // practice
        bucketsPerGame.emplace_back();
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

void ProgressManager::registerAnswer (int gameIndex, bool wasCorrect, float quality,
                                       int skillBucket)
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

    // Where this round sat in the exercise's own division of its subject.
    // Counted whether right or wrong: a miss rate needs both halves, and
    // "3 misses" means nothing without "out of how many".
    if (skillBucket >= 0 && skillBucket < maxSkillBuckets
        && gameIndex < (int) bucketsPerGame.size())
    {
        auto& bucket = bucketsPerGame[(size_t) gameIndex][(size_t) skillBucket];
        ++bucket.attempts;

        if (! wasCorrect)
            ++bucket.misses;
    }

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
            outcome.dailyChallengeJustCompleted = true;
        }
    }
    else
    {
        consecutiveCorrectPerGame[(size_t) gameIndex] = 0;
    }

    juce::ignoreUnused (quality);
    applyAnswerToProgress (gameIndex, wasCorrect, outcome);

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
                                    i < progressPerGame.size() ? progressPerGame[i].bestLevel : 1 });
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

int ProgressManager::getConsecutiveCorrectForGame (int gameIndex) const noexcept
{
    if (gameIndex < 0 || gameIndex >= (int) consecutiveCorrectPerGame.size())
        return 0;

    return consecutiveCorrectPerGame[(size_t) gameIndex];
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

void ProgressManager::applyAnswerToProgress (int gameIndex, bool wasCorrect,
                                              AnswerOutcome& outcome)
{
    auto& game = progressPerGame[(size_t) gameIndex];
    const auto before = game.level;

    if (wasCorrect)
    {
        // At the top step the run still counts up to full - the squares
        // stay lit - but there is nowhere further to go.
        if (++game.stepRun >= stepUpAfter)
        {
            if (game.level < maxLevel)
            {
                ++game.level;
                game.stepRun = 0;
            }
            else
            {
                game.stepRun = stepUpAfter - 1;
            }
        }
    }
    else
    {
        // One step easier, never below the first. This is the half that
        // makes it a measurement: a level that can only rise records how
        // long someone has played, not what they can hear.
        game.stepRun = 0;
        game.level = juce::jmax (1, game.level - 1);
    }

    if (game.level > game.bestLevel)
    {
        game.bestLevel = game.level;
        outcome.newBest = true;
    }

    outcome.leveledUp = game.level > before;
    outcome.steppedDown = game.level < before;
    outcome.level = game.level;
    outcome.bestLevel = game.bestLevel;
    outcome.stepRun = game.stepRun;

    if (game.level != before)
        gameManager.getGame (gameIndex).setDifficulty (game.level);
}

int ProgressManager::getLevelForGame (int gameIndex) const noexcept
{
    if (gameIndex < 0 || gameIndex >= (int) progressPerGame.size())
        return 1;

    return progressPerGame[(size_t) gameIndex].level;
}

int ProgressManager::getBestLevelForGame (int gameIndex) const noexcept
{
    if (gameIndex < 0 || gameIndex >= (int) progressPerGame.size())
        return 1;

    return progressPerGame[(size_t) gameIndex].bestLevel;
}

int ProgressManager::getStepRunForGame (int gameIndex) const noexcept
{
    if (gameIndex < 0 || gameIndex >= (int) progressPerGame.size())
        return 0;

    return progressPerGame[(size_t) gameIndex].stepRun;
}

float ProgressManager::getLevelProgressForGame (int gameIndex) const noexcept
{
    return (float) getStepRunForGame (gameIndex) / (float) stepUpAfter;
}

int ProgressManager::getMaxLevelReached() const noexcept
{
    auto highest = 1;
    for (const auto& game : progressPerGame)
        highest = juce::jmax (highest, game.bestLevel);

    return highest;
}

void ProgressManager::addPracticeSecond()
{
    ++practiceSeconds;

    // Written to disk once a minute rather than once a second. A
    // PropertiesFile save is a real file write, and doing one per second
    // for the whole time somebody is practising is exactly the sort of
    // background churn that gets a plugin blamed for a stuttering session.
    if (++unsavedPracticeSeconds >= 60)
    {
        unsavedPracticeSeconds = 0;
        saveState();
    }
}

int ProgressManager::getBucketAttempts (int gameIndex, int bucket) const
{
    if (gameIndex < 0 || gameIndex >= (int) bucketsPerGame.size()
        || bucket < 0 || bucket >= maxSkillBuckets)
        return 0;

    return bucketsPerGame[(size_t) gameIndex][(size_t) bucket].attempts;
}

int ProgressManager::getBucketMisses (int gameIndex, int bucket) const
{
    if (gameIndex < 0 || gameIndex >= (int) bucketsPerGame.size()
        || bucket < 0 || bucket >= maxSkillBuckets)
        return 0;

    return bucketsPerGame[(size_t) gameIndex][(size_t) bucket].misses;
}

int ProgressManager::getPreferredModeForGame (int gameIndex) const
{
    if (gameIndex < 0 || gameIndex >= (int) preferredModePerGame.size())
        return 0;

    return preferredModePerGame[(size_t) gameIndex];
}

void ProgressManager::setPreferredModeForGame (int gameIndex, int mode)
{
    if (gameIndex < 0 || gameIndex >= (int) preferredModePerGame.size())
        return;

    if (preferredModePerGame[(size_t) gameIndex] == mode)
        return;

    preferredModePerGame[(size_t) gameIndex] = mode;
    saveState();
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

    // The exercise with the lowest record - the weakest spot, which is
    // the one a coach would point at. Ties (every exercise on a fresh
    // install) are broken by a per-day seed, so the pick is stable across
    // reloads and still varies from day to day.
    {
        auto lowest = maxLevel + 1;
        for (const auto& game : progressPerGame)
            lowest = juce::jmin (lowest, game.bestLevel);

        juce::Array<int> weakest;
        for (int i = 0; i < (int) progressPerGame.size() && i < gameManager.getNumGames(); ++i)
            if (progressPerGame[(size_t) i].bestLevel == lowest)
                weakest.add (i);

        juce::Random dailyRandom ((juce::int64) todayIso.hashCode());
        dailyChallengeGameIndex = weakest.isEmpty() ? dailyRandom.nextInt (gameManager.getNumGames())
                                                    : weakest[dailyRandom.nextInt (weakest.size())];
    }

    for (auto& count : consecutiveCorrectPerGame)
        count = 0;

    saveState();
}

void ProgressManager::loadState()
{
    streakDays = properties->getIntValue ("streakDays", 0);
    practiceSeconds = properties->getIntValue ("practiceSeconds", 0);
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

        // A save from the points era has "level" and no "bestLevel": the
        // level it earned becomes both the starting step and the record,
        // so nobody who upgrades finds an exercise reset to 1.
        auto& gameProgress = progressPerGame[i];
        gameProgress.level     = juce::jlimit (1, maxLevel, properties->getIntValue (prefix + "level", 1));
        gameProgress.bestLevel = juce::jlimit (gameProgress.level, maxLevel,
                                               properties->getIntValue (prefix + "bestLevel", gameProgress.level));
        gameProgress.stepRun   = juce::jlimit (0, stepUpAfter - 1, properties->getIntValue (prefix + "stepRun", 0));

        if (i < favouritePerGame.size())
            favouritePerGame[i] = properties->getBoolValue (prefix + "favourite", false);

        if (i < preferredModePerGame.size())
            preferredModePerGame[i] = properties->getIntValue (prefix + "mode", 0);

        if (i < bucketsPerGame.size())
            for (int b = 0; b < maxSkillBuckets; ++b)
            {
                const auto key = prefix + "bucket" + juce::String (b);
                bucketsPerGame[i][(size_t) b].attempts = properties->getIntValue (key + ".att", 0);
                bucketsPerGame[i][(size_t) b].misses = properties->getIntValue (key + ".miss", 0);
            }
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
    properties->setValue ("practiceSeconds", practiceSeconds);
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
        properties->setValue (prefix + "level", gameProgress.level);
        properties->setValue (prefix + "bestLevel", gameProgress.bestLevel);
        properties->setValue (prefix + "stepRun", gameProgress.stepRun);

        if (i < favouritePerGame.size())
            properties->setValue (prefix + "favourite", (bool) favouritePerGame[i]);

        if (i < preferredModePerGame.size())
            properties->setValue (prefix + "mode", preferredModePerGame[i]);

        // Only what has been seen. Nine exercises times eight buckets
        // times two counters is 144 keys, and most stay zero for most
        // players - writing them all would treble the settings file to
        // record nothing.
        if (i < bucketsPerGame.size())
            for (int b = 0; b < maxSkillBuckets; ++b)
            {
                const auto& bucket = bucketsPerGame[i][(size_t) b];

                if (bucket.attempts > 0)
                {
                    const auto key = prefix + "bucket" + juce::String (b);
                    properties->setValue (key + ".att", bucket.attempts);
                    properties->setValue (key + ".miss", bucket.misses);
                }
            }
    }

    properties->setValue ("achievements", earnedAchievements.joinIntoString (","));

    properties->saveIfNeeded();
}

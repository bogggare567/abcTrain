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

    // Each exercise gets its own difficulty, not one shared number, and
    // starts asking where this player misses.
    for (int i = 0; i < gameManager.getNumGames(); ++i)
    {
        gameManager.getGame (i).setDifficulty (getLevelForGame (i));
        pushBucketWeights (i);
    }
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
    pushBucketWeights (gameIndex);

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
        if (++game.stepRun >= stepRule)
        {
            if (game.level < maxLevel)
            {
                ++game.level;
                game.stepRun = 0;
            }
            else
            {
                game.stepRun = stepRule - 1;
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

    // A turning point: the staircase moved the other way from last time.
    // The step it turned *at* is the reversal level.
    if (game.level != before)
    {
        const auto direction = game.level > before ? 1 : -1;

        if (game.lastDirection != 0 && direction != game.lastDirection)
        {
            game.reversals.push_back (before);

            if ((int) game.reversals.size() > reversalsKept)
                game.reversals.erase (game.reversals.begin());
        }

        game.lastDirection = direction;
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

float ProgressManager::getThresholdLevelForGame (int gameIndex) const noexcept
{
    if (gameIndex < 0 || gameIndex >= (int) progressPerGame.size())
        return -1.0f;

    const auto& reversals = progressPerGame[(size_t) gameIndex].reversals;

    if ((int) reversals.size() < reversalsForThreshold)
        return -1.0f;

    // An even number of the most recent ones, so as many peaks as troughs
    // go into the mean and neither end biases it (Levitt).
    const auto count = (int) reversals.size() - ((int) reversals.size() % 2);
    auto sum = 0.0f;

    for (int i = (int) reversals.size() - count; i < (int) reversals.size(); ++i)
        sum += (float) reversals[(size_t) i];

    return sum / (float) count;
}

std::vector<float> ProgressManager::computeBucketWeights (int gameIndex) const
{
    std::vector<float> weights;

    if (gameIndex < 0 || gameIndex >= gameManager.getNumGames()
        || gameIndex >= (int) bucketsPerGame.size())
        return weights;

    const auto numBuckets = juce::jmin (maxSkillBuckets, gameManager.getGame (gameIndex).getNumSkillBuckets());

    for (int b = 0; b < numBuckets; ++b)
    {
        const auto& bucket = bucketsPerGame[(size_t) gameIndex][(size_t) b];

        // Floor 0.25, plus the miss rate: a part missed every time comes up
        // five times as often as one never missed. Untried or barely tried:
        // the middle of that range, so it is explored rather than starved.
        const auto weight = bucket.attempts >= 3
                              ? 0.25f + (float) bucket.misses / (float) bucket.attempts
                              : 0.75f;
        weights.push_back (weight);
    }

    return weights;
}

void ProgressManager::pushBucketWeights (int gameIndex)
{
    if (gameIndex >= 0 && gameIndex < gameManager.getNumGames())
        gameManager.getGame (gameIndex).setBucketWeights (computeBucketWeights (gameIndex));
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
    return (float) getStepRunForGame (gameIndex) / (float) stepRule;
}

void ProgressManager::setStepUpAfter (int answersInARow)
{
    stepRule = juce::jlimit (2, 4, answersInARow);

    for (auto& game : progressPerGame)
        game.stepRun = juce::jmin (game.stepRun, stepRule - 1);
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
        gameProgress.stepRun   = juce::jlimit (0, stepRule - 1, properties->getIntValue (prefix + "stepRun", 0));
        gameProgress.lastDirection = juce::jlimit (-1, 1, properties->getIntValue (prefix + "direction", 0));
        gameProgress.reversals.clear();

        {
            juce::StringArray tokens;
            tokens.addTokens (properties->getValue (prefix + "reversals"), ",", "");
            tokens.removeEmptyStrings();

            for (const auto& token : tokens)
                if (gameProgress.reversals.size() < (size_t) reversalsKept)
                    gameProgress.reversals.push_back (juce::jlimit (1, maxLevel, token.getIntValue()));
        }

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
    ++changeCounter;

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
        properties->setValue (prefix + "direction", gameProgress.lastDirection);

        {
            juce::StringArray tokens;
            for (const auto r : gameProgress.reversals)
                tokens.add (juce::String (r));

            properties->setValue (prefix + "reversals", tokens.joinIntoString (","));
        }

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

// ---- sync ---------------------------------------------------------------------

juce::var ProgressManager::makeSyncSummary() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("v", 1);

    auto* games = new juce::DynamicObject();

    for (size_t i = 0; i < progressPerGame.size(); ++i)
    {
        const auto& p = progressPerGame[i];
        const auto& st = i < statsPerGame.size() ? statsPerGame[i] : GameStats {};

        if (st.roundsPlayed == 0 && p.bestLevel <= 1)
            continue;   // never played here: nothing to say, and nothing to overwrite elsewhere

        auto* g = new juce::DynamicObject();
        g->setProperty ("level", p.level);
        g->setProperty ("best", p.bestLevel);
        g->setProperty ("rounds", st.roundsPlayed);
        g->setProperty ("correct", st.correctAnswers);
        g->setProperty ("streak", st.bestStreak);
        g->setProperty ("survival", st.bestSurvivalScore);
        g->setProperty ("blitz", st.bestBlitzScore);
        games->setProperty (juce::String ((int) i), juce::var (g));
    }

    root->setProperty ("games", juce::var (games));
    root->setProperty ("streakDays", streakDays);
    root->setProperty ("practiceSeconds", practiceSeconds);
    root->setProperty ("achievements", earnedAchievements.joinIntoString (","));
    return juce::var (root);
}

bool ProgressManager::mergeSyncSummary (const juce::var& summary)
{
    if (! summary.isObject() || (int) summary.getProperty ("v", 0) != 1)
        return false;

    auto changed = false;
    const auto take = [&changed] (int& mine, int theirs)
    {
        if (theirs > mine)
        {
            mine = theirs;
            changed = true;
        }
    };

    if (auto* games = summary.getProperty ("games", {}).getDynamicObject())
    {
        for (const auto& entry : games->getProperties())
        {
            const auto index = entry.name.toString().getIntValue();

            if (! juce::isPositiveAndBelow (index, (int) progressPerGame.size())
                || entry.name.toString() != juce::String (index))
                continue;

            auto& p = progressPerGame[(size_t) index];
            auto& st = statsPerGame[(size_t) index];
            const auto& g = entry.value;

            const auto neverPlayedHere = st.roundsPlayed == 0 && p.bestLevel <= 1 && p.level <= 1;

            take (p.bestLevel, juce::jlimit (1, maxLevel, (int) g.getProperty ("best", 1)));

            if (neverPlayedHere)
            {
                const auto theirs = juce::jlimit (1, maxLevel, (int) g.getProperty ("level", 1));
                if (theirs != p.level)
                {
                    p.level = theirs;
                    changed = true;
                }
            }

            take (st.roundsPlayed, juce::jmax (0, (int) g.getProperty ("rounds", 0)));
            take (st.correctAnswers, juce::jmax (0, (int) g.getProperty ("correct", 0)));
            take (st.bestStreak, juce::jmax (0, (int) g.getProperty ("streak", 0)));
            take (st.bestSurvivalScore, juce::jmax (0, (int) g.getProperty ("survival", 0)));
            take (st.bestBlitzScore, juce::jmax (0, (int) g.getProperty ("blitz", 0)));

            // A record above the current step is fine (form dips); a
            // current step above the record is not.
            p.bestLevel = juce::jmax (p.bestLevel, p.level);
        }
    }

    take (streakDays, juce::jlimit (0, 100000, (int) summary.getProperty ("streakDays", 0)));
    take (practiceSeconds, juce::jmax (0, (int) summary.getProperty ("practiceSeconds", 0)));

    juce::StringArray theirs;
    theirs.addTokens (summary.getProperty ("achievements", "").toString(), ",", "");
    theirs.removeEmptyStrings();

    for (const auto& id : theirs)
        if (id.length() <= 64 && ! earnedAchievements.contains (id))
        {
            earnedAchievements.add (id);
            changed = true;
        }

    if (changed)
    {
        saveState();

        for (int i = 0; i < (int) progressPerGame.size(); ++i)
            pushBucketWeights (i);

        sendChangeMessage();
    }

    return changed;
}

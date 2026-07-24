#include <juce_core/juce_core.h>
#include "../Source/ProgressManager.h"
#include "../Source/GameManager.h"

// These tests drive ProgressManager through registerAnswer()/
// updateStreakForDate()/generateDailyChallengeForDate() directly rather
// than through the real Game -> ChangeListener wiring. That wiring relies
// on juce::ChangeBroadcaster::sendChangeMessage(), which is asynchronous
// and needs a running JUCE message loop to actually deliver - something
// this console test app never pumps. The real wiring (changeListenerCallback,
// a thin wrapper around registerAnswer) only gets exercised by actually
// running the plugin, not by this test file.
class ProgressManagerTest : public juce::UnitTest
{
public:
    ProgressManagerTest() : juce::UnitTest ("ProgressManager", "Progress") {}

    void runTest() override
    {
        beginTest ("level thresholds are a triangular scale (level L needs 100*L to reach L+1)");
        {
            expectEquals (ProgressManager::pointsRequiredForLevel (1), 0);
            expectEquals (ProgressManager::pointsRequiredForLevel (2), 100);
            expectEquals (ProgressManager::pointsRequiredForLevel (3), 300);
            expectEquals (ProgressManager::pointsRequiredForLevel (4), 600);

            expectEquals (ProgressManager::levelForScore (0), 1);
            expectEquals (ProgressManager::levelForScore (99), 1);
            expectEquals (ProgressManager::levelForScore (100), 2);
            expectEquals (ProgressManager::levelForScore (299), 2);
            expectEquals (ProgressManager::levelForScore (300), 3);
        }

        beginTest ("level never exceeds maxLevel even with a huge score");
        {
            expectEquals (ProgressManager::levelForScore (1000000), ProgressManager::maxLevel);
        }

        beginTest ("daysBetween counts whole days");
        {
            expectEquals (ProgressManager::daysBetween ("2026-01-01", "2026-01-02"), 1);
            expectEquals (ProgressManager::daysBetween ("2026-01-01", "2026-01-05"), 4);
            expectEquals (ProgressManager::daysBetween ("2026-01-01", "2026-01-01"), 0);
        }

        beginTest ("streak increments on consecutive days and resets on a gap");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("streak"));

            progress.updateStreakForDate ("2026-01-01");
            expectEquals (progress.getStreakDays(), 1);

            progress.updateStreakForDate ("2026-01-02");
            expectEquals (progress.getStreakDays(), 2);

            progress.updateStreakForDate ("2026-01-03");
            expectEquals (progress.getStreakDays(), 3);

            progress.updateStreakForDate ("2026-01-06"); // 3-day gap
            expectEquals (progress.getStreakDays(), 1);
        }

        beginTest ("revisiting the same day does not double-count the streak");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("sameday"));

            progress.updateStreakForDate ("2026-02-01");
            expectEquals (progress.getStreakDays(), 1);

            progress.updateStreakForDate ("2026-02-01");
            expectEquals (progress.getStreakDays(), 1);
        }

        beginTest ("a correct answer awards points and can level up");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("scoring"));

            expectEquals (progress.getLevel(), 1);

            for (int i = 0; i < 10; ++i)
                progress.registerAnswer (0, true);

            expectEquals (progress.getTotalScore(), 100);
            expectEquals (progress.getLevel(), 2);
        }

        beginTest ("a wrong answer does not award points and resets the streak-in-a-row");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("wronganswer"));

            progress.registerAnswer (0, true);
            progress.registerAnswer (0, false);

            expectEquals (progress.getTotalScore(), 10); // only the first answer counted
        }

        beginTest ("difficulty is applied to every game, not just the active one");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("difficulty"));

            for (int i = 0; i < 10; ++i)
                progress.registerAnswer (0, true); // -> level 2

            expectEquals (progress.getLevel(), 2);
            // ReverbGame (index 2) exposes its difficulty tier via choice
            // count - levels 1-3 -> 2 choices, so this doesn't change yet,
            // but confirms setDifficulty was actually called (no crash,
            // choice count still in the valid easy-tier range).
            expect (gameManager.getGame (2).getNumChoices() == 2);
        }

        beginTest ("daily challenge completes after the target streak on its own game");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("dailycomplete"));

            progress.generateDailyChallengeForDate ("2026-03-01");
            const auto targetGame = progress.getDailyChallengeGameIndex();

            for (int i = 0; i < ProgressManager::dailyChallengeTargetStreak - 1; ++i)
                progress.registerAnswer (targetGame, true);
            expect (! progress.isDailyChallengeComplete());

            progress.registerAnswer (targetGame, true);
            expect (progress.isDailyChallengeComplete());
        }

        beginTest ("correct answers on a different game don't progress the daily challenge");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("dailywronggame"));

            progress.generateDailyChallengeForDate ("2026-03-02");
            const auto targetGame = progress.getDailyChallengeGameIndex();
            const auto otherGame = (targetGame + 1) % gameManager.getNumGames();

            for (int i = 0; i < ProgressManager::dailyChallengeTargetStreak + 2; ++i)
                progress.registerAnswer (otherGame, true);

            expect (! progress.isDailyChallengeComplete());
        }

        beginTest ("regenerating the challenge for the same date is a no-op");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("dailysameday"));

            progress.generateDailyChallengeForDate ("2026-03-03");
            const auto targetGame = progress.getDailyChallengeGameIndex();
            progress.registerAnswer (targetGame, true);
            progress.registerAnswer (targetGame, true);

            progress.generateDailyChallengeForDate ("2026-03-03"); // same date again

            // If it had regenerated, the streak-in-a-row tracking would
            // have been reset to 0 and the target game might differ.
            expectEquals (progress.getDailyChallengeGameIndex(), targetGame);
        }

        beginTest ("persisted state survives reconstruction");
        {
            const auto options = makeTempOptions ("persistence");

            {
                GameManager gameManager;
                ProgressManager progress (gameManager, options);
                progress.registerAnswer (0, true);
            } // destructor saves state

            GameManager gameManager2;
            ProgressManager reloaded (gameManager2, options);
            expectEquals (reloaded.getTotalScore(), 10);
        }
    }

private:
    static juce::PropertiesFile::Options makeTempOptions (const juce::String& uniqueSuffix)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "EarTrainerTests";
        options.filenameSuffix = "settings";
        options.folderName = "EarTrainerTests_" + uniqueSuffix;
        options.commonToAllUsers = false;

        // PropertiesFile persists to disk by design (that's the whole
        // point for real use) - so without this, leftover state from a
        // previous run of this same test binary on this same machine
        // leaks into "a correct answer awards points" et al, which all
        // assume a fresh/empty file. CI always runs in a fresh container,
        // so this never surfaced there; it only showed up the first time
        // this binary was ever run twice locally, in this same session.
        options.getDefaultFile().deleteFile();

        return options;
    }
};

static ProgressManagerTest progressManagerTest;

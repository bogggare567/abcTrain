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

        beginTest ("points and levels are per exercise, and everyone starts at 1");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("perexercise"));

            for (int i = 0; i < gameManager.getNumGames(); ++i)
            {
                expectEquals (progress.getLevelForGame (i), 1);
                expectEquals (progress.getPointsForGame (i), 0);
            }

            for (int i = 0; i < 10; ++i)
                progress.registerAnswer (0, true);

            // A correct answer is worth pointsPerCorrectAnswer plus the
            // precision bonus scaled by quality; registerAnswer defaults
            // quality to 1, so these are full marks.
            expectEquals (progress.getPointsForGame (0),
                           10 * (ProgressManager::pointsPerCorrectAnswer
                                  + ProgressManager::precisionBonusPoints));
            expectEquals (progress.getPointsForGame (1), 0);
            expectEquals (progress.getLevelForGame (1), 1);
        }

        beginTest ("points open the promotion, the test closes it");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("promotion"));

            // Interleave a wrong answer so nothing is a lucky run: points
            // accumulate, the promotion opens on the answer that crosses
            // the threshold, and the test starts counting from there.
            for (int i = 0; i < 10; ++i)
            {
                progress.registerAnswer (0, true);
                progress.registerAnswer (0, false);
            }

            expect (progress.isPromotionPendingForGame (0), "promotion should be open");
            expectEquals (progress.getLevelForGame (0), 1, "the test has not been passed yet");
            expectEquals (progress.getPromotionStreakForGame (0), 0);

            // Four in a row is not five.
            for (int i = 0; i < ProgressManager::promotionTestLength - 1; ++i)
                progress.registerAnswer (0, true);

            expectEquals (progress.getLevelForGame (0), 1);
            expectEquals (progress.getPromotionStreakForGame (0),
                           ProgressManager::promotionTestLength - 1);

            // One wrong answer costs the run, not the level or the points.
            const auto pointsBefore = progress.getPointsForGame (0);
            progress.registerAnswer (0, false);
            expectEquals (progress.getPromotionStreakForGame (0), 0);
            expectEquals (progress.getLevelForGame (0), 1);
            expectEquals (progress.getPointsForGame (0), pointsBefore);
            expect (progress.isPromotionPendingForGame (0), "the promotion stays open");

            for (int i = 0; i < ProgressManager::promotionTestLength; ++i)
                progress.registerAnswer (0, true);

            expectEquals (progress.getLevelForGame (0), 2);
            expect (! progress.isPromotionPendingForGame (0));
            expectEquals (progress.getPromotionStreakForGame (0), 0);
        }

        beginTest ("onAnswerScored reports what each answer produced (ADR 029)");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("outcome"));

            ProgressManager::AnswerOutcome last;
            int lastIndex = -1;
            progress.onAnswerScored = [&] (int index, const ProgressManager::AnswerOutcome& outcome)
            {
                lastIndex = index;
                last = outcome;
            };

            // A dead-centre correct answer is worth base + full precision.
            progress.registerAnswer (0, true, 1.0f);
            expectEquals (lastIndex, 0);
            expect (last.wasCorrect);
            expectEquals (last.pointsAwarded,
                           ProgressManager::pointsPerCorrectAnswer + ProgressManager::precisionBonusPoints);
            expect (! last.leveledUp);
            expect (! last.promotionJustOpened);

            // A wrong answer awards nothing and, with no test live, fails
            // nothing either.
            progress.registerAnswer (0, false);
            expect (! last.wasCorrect);
            expectEquals (last.pointsAwarded, 0);
            expect (! last.promotionJustFailed, "no test was live to fail");

            // Grind to the threshold: the answer that crosses it reports
            // the promotion opening; the fifth in a row after that reports
            // the level-up, exactly once.
            auto sawOpen = false;
            auto sawLevelUp = false;

            for (int i = 0; i < 40 && ! sawLevelUp; ++i)
            {
                progress.registerAnswer (0, true, 1.0f);
                sawOpen = sawOpen || last.promotionJustOpened;
                sawLevelUp = sawLevelUp || last.leveledUp;
            }

            expect (sawOpen, "crossing the threshold should report the test opening");
            expect (sawLevelUp, "passing the test should report the level-up");
            expectEquals (last.level, 2, "the outcome carries the level after the answer");
            expect (! last.promotionPending, "the test closed with the level");

            // A wrong answer during a live test reports the failure.
            for (int i = 0; i < 30 && ! progress.isPromotionPendingForGame (0); ++i)
                progress.registerAnswer (0, true, 1.0f);

            if (progress.isPromotionPendingForGame (0))
            {
                progress.registerAnswer (0, true, 1.0f);   // one into the test
                progress.registerAnswer (0, false);
                expect (last.promotionJustFailed, "a wrong answer mid-test should say so");
            }
        }

        beginTest ("levelling one exercise does not touch another's difficulty");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("isolation"));

            // ReverbGame (index 2) reports its tier through its choice
            // count: 2 at levels 1-3, 3 at 4-6, 4 at 7+. Grinding the EQ
            // game must leave it exactly where it started.
            expect (gameManager.getGame (2).getNumChoices() == 2);

            for (int i = 0; i < 200; ++i)
                progress.registerAnswer (0, true);

            expect (progress.getLevelForGame (0) > 1, "the EQ game should have levelled");
            expectEquals (progress.getLevelForGame (2), 1);
            expect (gameManager.getGame (2).getNumChoices() == 2);
        }

        beginTest ("a wrong answer costs no points");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("wronganswer"));

            progress.registerAnswer (0, true);
            progress.registerAnswer (0, false);

            const auto full = ProgressManager::pointsPerCorrectAnswer
                                  + ProgressManager::precisionBonusPoints;

            expectEquals (progress.getPointsForGame (0), full);
            expectEquals (progress.getTotalScore(), full);
        }

        beginTest ("precision scales the points a correct answer is worth");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("precision"));

            // Dead on the target is worth the full rate; scraping the edge
            // of the accept band is worth the base rate and nothing more.
            progress.registerAnswer (0, true, 1.0f);
            expectEquals (progress.getPointsForGame (0),
                           ProgressManager::pointsPerCorrectAnswer
                               + ProgressManager::precisionBonusPoints);

            progress.registerAnswer (1, true, 0.0f);
            expectEquals (progress.getPointsForGame (1),
                           ProgressManager::pointsPerCorrectAnswer);

            // Out-of-range quality is clamped rather than trusted - it
            // comes from a Game, and a game with a bug should not be able
            // to award itself a thousand points.
            progress.registerAnswer (2, true, 9.0f);
            expectEquals (progress.getPointsForGame (2),
                           ProgressManager::pointsPerCorrectAnswer
                               + ProgressManager::precisionBonusPoints);
        }

        beginTest ("getTotalScore sums every exercise");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("totalscore"));

            progress.registerAnswer (0, true);
            progress.registerAnswer (3, true);
            progress.registerAnswer (7, true);

            expectEquals (progress.getTotalScore(),
                           3 * (ProgressManager::pointsPerCorrectAnswer
                                 + ProgressManager::precisionBonusPoints));
        }

        beginTest ("an out-of-range game index is a harmless miss, not a crash");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("outofrange"));

            progress.registerAnswer (999, true);
            progress.registerAnswer (-1, true);

            expectEquals (progress.getTotalScore(), 0);
            expectEquals (progress.getLevelForGame (999), 1);
            expectEquals (progress.getPointsForGame (-1), 0);
            expect (! progress.isPromotionPendingForGame (999));
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
            expectEquals (reloaded.getTotalScore(),
                           ProgressManager::pointsPerCorrectAnswer
                               + ProgressManager::precisionBonusPoints);
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

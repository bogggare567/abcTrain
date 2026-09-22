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

        beginTest ("levels are per exercise, and everyone starts at 1");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("perexercise"));

            for (int i = 0; i < gameManager.getNumGames(); ++i)
            {
                expectEquals (progress.getLevelForGame (i), 1);
                expectEquals (progress.getBestLevelForGame (i), 1);
                expectEquals (progress.getStepRunForGame (i), 0);
            }

            for (int i = 0; i < ProgressManager::stepUpAfter; ++i)
                progress.registerAnswer (0, true);

            expectEquals (progress.getLevelForGame (0), 2);
            expectEquals (progress.getLevelForGame (1), 1, "another exercise is untouched");
        }

        beginTest ("the staircase: three right is a step harder, one wrong a step easier");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("staircase"));

            // Two right: the run counts, the step does not move yet.
            progress.registerAnswer (0, true);
            progress.registerAnswer (0, true);
            expectEquals (progress.getLevelForGame (0), 1);
            expectEquals (progress.getStepRunForGame (0), 2);

            // A wrong answer at the bottom step clears the run but cannot
            // go below 1.
            progress.registerAnswer (0, false);
            expectEquals (progress.getLevelForGame (0), 1);
            expectEquals (progress.getStepRunForGame (0), 0);

            for (int i = 0; i < 3 * ProgressManager::stepUpAfter; ++i)
                progress.registerAnswer (0, true);

            expectEquals (progress.getLevelForGame (0), 4);
            expectEquals (progress.getBestLevelForGame (0), 4);

            progress.registerAnswer (0, false);
            expectEquals (progress.getLevelForGame (0), 3, "one wrong answer is one step easier");
            expectEquals (progress.getBestLevelForGame (0), 4, "the record never drops");
        }

        beginTest ("the top step holds, and the run stays full there");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("topstep"));

            for (int i = 0; i < 100; ++i)
                progress.registerAnswer (0, true);

            expectEquals (progress.getLevelForGame (0), ProgressManager::maxLevel);
            expectEquals (progress.getStepRunForGame (0), ProgressManager::stepUpAfter - 1);
        }

        beginTest ("a staircase settles near 79% correct");
        {
            // The whole reason for 3-down/1-up (Levitt 1971): a listener
            // whose accuracy falls as the step rises ends up hovering where
            // p^3 = 0.5, i.e. p ~= 0.794. Simulated here with a listener
            // who is perfect at step 1 and drops 6% a step.
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("converge"));
            juce::Random random (1234);

            int correct = 0, counted = 0;
            for (int i = 0; i < 4000; ++i)
            {
                const auto p = 1.0f - 0.06f * (float) (progress.getLevelForGame (0) - 1);
                const auto right = random.nextFloat() < p;
                progress.registerAnswer (0, right);

                if (i >= 500) { ++counted; if (right) ++correct; }
            }

            const auto rate = (float) correct / (float) counted;
            expect (rate > 0.74f && rate < 0.85f, "settled at " + juce::String (rate, 3));
        }

        beginTest ("the threshold is the mean of the reversals, and the record overstates it (ADR 040)");
        {
            // Same simulated listener: right with p = 1 - 0.06 (step - 1),
            // so p = 0.794 - where 3-down/1-up converges - at step 4.43.
            const auto options = makeTempOptions ("threshold");
            GameManager gameManager;
            ProgressManager progress (gameManager, options);
            juce::Random random (99);

            expect (! progress.hasThresholdForGame (0));

            for (int i = 0; i < 600; ++i)
            {
                const auto p = 1.0f - 0.06f * (float) (progress.getLevelForGame (0) - 1);
                progress.registerAnswer (0, random.nextFloat() < p);
            }

            expect (progress.hasThresholdForGame (0));
            const auto threshold = progress.getThresholdLevelForGame (0);
            logMessage ("threshold " + juce::String (threshold, 2) + ", record "
                        + juce::String (progress.getBestLevelForGame (0)));

            expectWithinAbsoluteError (threshold, 4.43f, 1.2f);
            expect ((float) progress.getBestLevelForGame (0) > threshold,
                    "the record is the top of a random walk and should sit above the threshold");

            // It survives a restart.
            GameManager gameManager2;
            ProgressManager reloaded (gameManager2, options);
            expectWithinAbsoluteError (reloaded.getThresholdLevelForGame (0), threshold, 1.0e-4f);
        }

        beginTest ("bucket weights follow the misses, with a floor (ADR 040)");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("weights"));

            // Bucket 1 always missed, bucket 2 never, bucket 0 barely tried.
            for (int i = 0; i < 6; ++i)
            {
                progress.registerAnswer (0, false, 1.0f, 1);
                progress.registerAnswer (0, true, 1.0f, 2);
            }
            progress.registerAnswer (0, false, 1.0f, 0);

            const auto weights = progress.computeBucketWeights (0);
            expect (weights.size() >= 3);
            expectWithinAbsoluteError (weights[1], 1.25f, 1.0e-4f);
            expectWithinAbsoluteError (weights[2], 0.25f, 1.0e-4f);
            expectWithinAbsoluteError (weights[0], 0.75f, 1.0e-4f);

            // And they reached the game.
            expectWithinAbsoluteError (gameManager.getGame (0).bucketWeight (1), 1.25f, 1.0e-4f);
        }

        beginTest ("onAnswerScored reports each step (ADR 035)");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("outcome"));

            ProgressManager::AnswerOutcome last;
            int calls = 0;
            progress.onAnswerScored = [&] (int, const ProgressManager::AnswerOutcome& o) { last = o; ++calls; };

            progress.registerAnswer (0, true);
            expectEquals (calls, 1);
            expect (last.wasCorrect && ! last.leveledUp && ! last.steppedDown);
            expectEquals (last.stepRun, 1);

            progress.registerAnswer (0, true);
            progress.registerAnswer (0, true);
            expect (last.leveledUp && last.newBest);
            expectEquals (last.level, 2);
            expectEquals (last.bestLevel, 2);

            progress.registerAnswer (0, false);
            expect (last.steppedDown && ! last.newBest);
            expectEquals (last.level, 1);
            expectEquals (last.bestLevel, 2);
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

        beginTest ("an out-of-range game index is a harmless miss, not a crash");
        {
            GameManager gameManager;
            ProgressManager progress (gameManager, makeTempOptions ("outofrange"));

            progress.registerAnswer (999, true);
            progress.registerAnswer (-1, true);

            expectEquals (progress.getLevelForGame (999), 1);
            expectEquals (progress.getBestLevelForGame (-1), 1);
            expectEquals (progress.getStepRunForGame (999), 0);
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
                for (int i = 0; i < 2 * ProgressManager::stepUpAfter + 1; ++i)
                    progress.registerAnswer (0, true);
                progress.registerAnswer (0, false);
            } // destructor saves state

            GameManager gameManager2;
            ProgressManager reloaded (gameManager2, options);
            expectEquals (reloaded.getLevelForGame (0), 2);
            expectEquals (reloaded.getBestLevelForGame (0), 3);
            expectEquals (reloaded.getStepRunForGame (0), 0);
        }

        beginTest ("a points-era save keeps its level as both step and record");
        {
            const auto options = makeTempOptions ("migrate");
            {
                juce::PropertiesFile old (options);
                old.setValue ("game0.points", 1200);
                old.setValue ("game0.level", 5);
                old.setValue ("game0.promotionPending", true);
                old.saveIfNeeded();
            }

            GameManager gameManager;
            ProgressManager progress (gameManager, options);
            expectEquals (progress.getLevelForGame (0), 5);
            expectEquals (progress.getBestLevelForGame (0), 5);
            expectEquals (progress.getLevelForGame (1), 1);
        }

        beginTest ("each exercise remembers its own mode, and it survives a reload");
        {
            // The mode used to live only in SessionManager, of which there
            // is one - so leaving a Blitz run, going home and opening a
            // different exercise dropped you into a Blitz countdown on a
            // skill you had only ever practised. It is a property of how
            // you are working on *that* exercise.
            const auto options = makeTempOptions ("permode");

            {
                GameManager gameManager;
                ProgressManager progress (gameManager, options);

                expectEquals (progress.getPreferredModeForGame (0), 0);
                expectEquals (progress.getPreferredModeForGame (3), 0);

                progress.setPreferredModeForGame (0, 2);   // blitz here
                progress.setPreferredModeForGame (3, 1);   // survival there

                expectEquals (progress.getPreferredModeForGame (0), 2);
                expectEquals (progress.getPreferredModeForGame (3), 1);

                // and every other exercise is untouched by both
                expectEquals (progress.getPreferredModeForGame (1), 0);

                // out of range is a miss, not a crash or a write past the end
                expectEquals (progress.getPreferredModeForGame (-1), 0);
                expectEquals (progress.getPreferredModeForGame (999), 0);
                progress.setPreferredModeForGame (999, 2);
            }

            GameManager gameManager2;
            ProgressManager reloaded (gameManager2, options);
            expectEquals (reloaded.getPreferredModeForGame (0), 2);
            expectEquals (reloaded.getPreferredModeForGame (3), 1);
            expectEquals (reloaded.getPreferredModeForGame (1), 0);
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

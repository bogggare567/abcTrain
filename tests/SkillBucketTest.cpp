#include <juce_core/juce_core.h>

#include "../Source/ProgressManager.h"
#include "../Source/GameManager.h"
#include "../Source/Games/EQGame.h"
#include "../Source/Games/PanGame.h"
#include "../Source/Games/DelayGame.h"
#include "../Source/Games/DBGame.h"
#include "../Source/Games/ReverbGame.h"
#include "../Source/Games/CompressionGame.h"
#include "../Source/Games/DistortionGame.h"
#include "../Source/Games/StereoWidthGame.h"
#include "../Source/Games/FrequencyRangeGame.h"

// The map of where somebody's misses land.
//
// Score and accuracy report a run that is already over. This is the one
// part of a results screen that changes what a player does tomorrow, so
// the thing it must never do is lie: a bucket named as your weak spot has
// to actually be where you missed, and a bucket you have never played has
// to read as untried rather than as perfect.
class SkillBucketTest : public juce::UnitTest
{
public:
    SkillBucketTest() : juce::UnitTest ("Skill buckets", "Games") {}

    void runTest() override
    {
        beginTest ("every exercise divides its own subject, and labels every part");
        {
            EQGame eq; PanGame pan; DelayGame delay; DBGame db; ReverbGame reverb;
            CompressionGame comp; DistortionGame dist; StereoWidthGame width;
            FrequencyRangeGame range;

            for (Game* game : { (Game*) &eq, (Game*) &pan, (Game*) &delay, (Game*) &db,
                                 (Game*) &reverb, (Game*) &comp, (Game*) &dist,
                                 (Game*) &width, (Game*) &range })
            {
                prepare (*game);
                const auto n = game->getNumSkillBuckets();

                expect (n > 1, juce::String (game->getName()) + " offers no map");
                expect (n <= ProgressManager::maxSkillBuckets,
                         juce::String (game->getName()) + " has more buckets than storage: "
                             + juce::String (n));

                for (int b = 0; b < n; ++b)
                    expect (game->getSkillBucketLabel (b).isNotEmpty(),
                             juce::String (game->getName()) + " bucket " + juce::String (b)
                                 + " has no name");
            }
        }

        beginTest ("an answered round always lands in a real bucket");
        {
            // Across every level, because several games draw their round
            // differently at different difficulties - and a bucket index
            // out of range would be silently dropped by ProgressManager,
            // so the map would just be quietly wrong rather than crash.
            EQGame eq; PanGame pan; DelayGame delay; DBGame db; ReverbGame reverb;
            CompressionGame comp; DistortionGame dist; StereoWidthGame width;
            FrequencyRangeGame range;

            for (Game* game : { (Game*) &eq, (Game*) &pan, (Game*) &delay, (Game*) &db,
                                 (Game*) &reverb, (Game*) &comp, (Game*) &dist,
                                 (Game*) &width, (Game*) &range })
            {
                prepare (*game);
                const auto n = game->getNumSkillBuckets();

                for (int level = 1; level <= 10; ++level)
                {
                    game->setDifficulty (level);

                    for (int round = 0; round < 12; ++round)
                    {
                        game->newRound();

                        // Nothing recorded before an answer: a fresh round
                        // is not a data point.
                        expectEquals (game->getSkillBucketForRound(), -1,
                                       juce::String (game->getName())
                                           + " reported a bucket before it was answered");

                        if (game->usesContinuousScale())
                            game->submitNormalisedAnswer (0.5f);
                        else
                            game->submitAnswer (0);

                        const auto bucket = game->getSkillBucketForRound();
                        expect (bucket >= 0 && bucket < n,
                                 juce::String (game->getName()) + " at level "
                                     + juce::String (level) + " gave bucket "
                                     + juce::String (bucket) + " of " + juce::String (n));
                    }
                }
            }
        }

        beginTest ("attempts and misses are counted per bucket, and only where told");
        {
            const auto options = makeTempOptions ("buckets");
            GameManager gameManager;
            ProgressManager progress (gameManager, options);

            // Four wrong out of six in bucket 2; three right in bucket 5.
            for (int i = 0; i < 6; ++i)
                progress.registerAnswer (0, i >= 4, 1.0f, 2);

            for (int i = 0; i < 3; ++i)
                progress.registerAnswer (0, true, 1.0f, 5);

            expectEquals (progress.getBucketAttempts (0, 2), 6);
            expectEquals (progress.getBucketMisses (0, 2), 4);
            expectEquals (progress.getBucketAttempts (0, 5), 3);
            expectEquals (progress.getBucketMisses (0, 5), 0);

            // An untouched bucket reads as untried, which is a different
            // thing from perfect and has to stay distinguishable.
            expectEquals (progress.getBucketAttempts (0, 1), 0);
            expectEquals (progress.getBucketMisses (0, 1), 0);

            // And nothing leaks into a neighbouring exercise.
            expectEquals (progress.getBucketAttempts (1, 2), 0);
        }

        beginTest ("no bucket, or an impossible one, records nothing and does not crash");
        {
            const auto options = makeTempOptions ("bucketsguard");
            GameManager gameManager;
            ProgressManager progress (gameManager, options);

            const auto roundsBefore = progress.getStatsForGame (0).roundsPlayed;

            progress.registerAnswer (0, false, 1.0f, -1);                                  // no map
            progress.registerAnswer (0, false, 1.0f, ProgressManager::maxSkillBuckets);    // past the end
            progress.registerAnswer (0, false, 1.0f, 9999);
            progress.registerAnswer (0, false, 1.0f, -50);

            // The answers themselves still count - only the map entry is
            // skipped. Dropping the round as well would make the accuracy
            // on the same screen disagree with the map beside it.
            expectEquals (progress.getStatsForGame (0).roundsPlayed, roundsBefore + 4);

            for (int b = 0; b < ProgressManager::maxSkillBuckets; ++b)
                expectEquals (progress.getBucketAttempts (0, b), 0,
                               "a bucket recorded something it was never given");

            expectEquals (progress.getBucketAttempts (0, -1), 0);
            expectEquals (progress.getBucketMisses (0, 9999), 0);
        }

        beginTest ("the map survives a reload");
        {
            const auto options = makeTempOptions ("bucketspersist");

            {
                GameManager gameManager;
                ProgressManager progress (gameManager, options);

                for (int i = 0; i < 7; ++i)
                    progress.registerAnswer (3, i >= 5, 1.0f, 1);
            }

            GameManager gameManager2;
            ProgressManager reloaded (gameManager2, options);

            expectEquals (reloaded.getBucketAttempts (3, 1), 7);
            expectEquals (reloaded.getBucketMisses (3, 1), 5);
            expectEquals (reloaded.getBucketAttempts (3, 0), 0);
        }

        beginTest ("a frequency lands in the range that contains it");
        {
            // The lookup EQGame borrows from FrequencyRangeGame. Getting
            // this wrong would name the wrong part of the spectrum as
            // somebody's weak spot, which is worse than saying nothing.
            expectEquals (FrequencyRangeGame::rangeIndexFor (30.0f), 0);      // Sub-bass
            expectEquals (FrequencyRangeGame::rangeIndexFor (100.0f), 1);     // Bass
            expectEquals (FrequencyRangeGame::rangeIndexFor (300.0f), 2);     // Low-mids
            expectEquals (FrequencyRangeGame::rangeIndexFor (1000.0f), 3);    // Mids
            expectEquals (FrequencyRangeGame::rangeIndexFor (3000.0f), 4);    // High-mids
            expectEquals (FrequencyRangeGame::rangeIndexFor (5000.0f), 5);    // Presence
            expectEquals (FrequencyRangeGame::rangeIndexFor (12000.0f), 6);   // Air

            // Off both ends rather than reading past the table.
            expectEquals (FrequencyRangeGame::rangeIndexFor (1.0f), 0);
            expectEquals (FrequencyRangeGame::rangeIndexFor (44000.0f),
                           FrequencyRangeGame::numRanges - 1);
        }
    }

private:
    static void prepare (Game& game)
    {
        juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
        game.prepare (spec);
    }

    static juce::PropertiesFile::Options makeTempOptions (const juce::String& suffix)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "EarTrainerTests";
        options.filenameSuffix = "settings";
        options.folderName = "EarTrainerTests_" + suffix;
        options.commonToAllUsers = false;
        options.getDefaultFile().deleteFile();   // same reason as ProgressManagerTest
        return options;
    }
};

static SkillBucketTest skillBucketTest;

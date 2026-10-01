#include <juce_core/juce_core.h>
#include "../Source/GameManager.h"
#include "../Source/SessionManager.h"
#include "../Source/PerceptualModel.h"

// Battles with damage (ADR 049): the error on each exercise's own scale, the
// damage curve, HP, and bots that miss by a distribution rather than flip a
// coin. Statistical checks use thousands of draws and margins far wider than
// their sampling error - never one random outcome.
class BattleTest : public juce::UnitTest
{
public:
    BattleTest() : juce::UnitTest ("Battle", "Live") {}

    void runTest() override
    {
        beginTest ("damage: nothing for a dead-on answer, little at the edge, most for a wild one, smooth between");
        {
            expectEquals (SessionManager::damageFor (0.0f), 0.0f);
            expectEquals (SessionManager::damageFor (0.6f), 0.0f);
            expect (SessionManager::damageFor (1.0f) < 3.0f, "at the edge of 'right': " + juce::String (SessionManager::damageFor (1.0f)));
            expect (SessionManager::damageFor (2.0f) > 15.0f, "twice the tolerance: " + juce::String (SessionManager::damageFor (2.0f)));
            expect (SessionManager::damageFor (Game::missedError) > 19.5f);
            expect (SessionManager::damageFor (100.0f) <= SessionManager::maxDamage);

            // The same numbers scripts/abctrain-battles-test.mjs checks on the
            // server: the two curves have to agree.
            for (auto [e, d] : { std::pair { 1.0f, 2.128f }, std::pair { 2.0f, 17.583f }, std::pair { 3.0f, 19.973f } })
                expectWithinAbsoluteError (SessionManager::damageFor (e), d, 0.002f);

            float previous = 0.0f;
            for (float e = 0.0f; e <= 4.0f; e += 0.05f)
            {
                const auto d = SessionManager::damageFor (e);
                expect (d >= previous, "never less damage for a bigger miss");
                expect (d - previous < 1.5f, "no step at " + juce::String (e));
                previous = d;
            }
        }

        beginTest ("HP: a wild miss costs more than a near one; five wild ones end the battle early");
        {
            SessionManager s;
            s.setMode (SessionManager::Mode::duel);
            s.startRun();
            s.registerBattleRound (1.2f, 0.1f);
            const auto afterNear = s.getPlayerHp();
            expect (afterNear < SessionManager::startHp && afterNear > 90.0f, juce::String (afterNear));
            expectEquals (s.getOpponentHp(), SessionManager::startHp);

            int rounds = 1;
            while (s.registerBattleRound (Game::missedError, 0.0f))
                ++rounds;
            ++rounds;

            expect (! s.isRunActive());
            expect (rounds <= 6, "over before ten rounds: " + juce::String (rounds));
            expectEquals (s.getPlayerHp(), 0.0f);
            expect (s.getDuelOutcome() == SessionManager::Outcome::lost);
        }

        beginTest ("ten near misses do not knock anyone out");
        {
            SessionManager s;
            s.setMode (SessionManager::Mode::duel);
            s.startRun();
            for (int i = 0; i < SessionManager::duelRounds; ++i)
                s.registerBattleRound (1.0f, 0.5f);
            expect (! s.isRunActive());
            expect (s.getPlayerHp() > 70.0f, juce::String (s.getPlayerHp()));
            expect (s.getDuelOutcome() == SessionManager::Outcome::lost, "still lost - by HP, narrowly");
        }

        beginTest ("perceptual distance: octaves, dB, ms, % - and relative to the tolerance");
        {
            GameManager gm;

            // Band: one octave on a 20 Hz - 20 kHz log axis is 1 / 9.97 of it.
            {
                auto& g = gm.getGame (0);
                g.setDifficulty (5);
                g.newRound();
                const auto target = g.getCorrectNormalised();
                const auto chosen = target + (target < 0.5f ? 1.0f : -1.0f) / 9.9658f;
                g.submitNormalisedAnswer (chosen);
                expectWithinAbsoluteError (g.answerErrorNative(), 1.0f, 0.01f);
                expectEquals (g.answerErrorUnit(), juce::String ("oct"));
                expectWithinAbsoluteError (g.answerErrorRelative(), std::abs (chosen - target) / g.getToleranceNormalised(), 1.0e-4f);
            }

            // Gain: 3 dB off on a -9..+9 axis.
            {
                auto& g = gm.getGame (7);
                g.setDifficulty (5);
                g.newRound();
                const auto target = g.getCorrectNormalised();
                g.submitNormalisedAnswer (target + (target < 0.5f ? 3.0f : -3.0f) / 18.0f);
                expectWithinAbsoluteError (g.answerErrorNative(), 3.0f, 0.01f);
                expectEquals (g.answerErrorUnit(), juce::String ("dB"));
            }

            // Pan: a quarter of the axis is half a side - 50 %.
            {
                auto& g = gm.getGame (3);
                g.setDifficulty (5);
                g.newRound();
                const auto target = g.getCorrectNormalised();
                g.submitNormalisedAnswer (target + (target < 0.5f ? 0.25f : -0.25f));
                expectWithinAbsoluteError (g.answerErrorNative(), 50.0f, 0.1f);
            }

            // Delay: dead on is 0 ms and 0 relative.
            {
                auto& g = gm.getGame (4);
                g.setDifficulty (5);
                g.newRound();
                g.submitNormalisedAnswer (g.getCorrectNormalised());
                expectWithinAbsoluteError (g.answerErrorNative(), 0.0f, 0.5f);
                expectWithinAbsoluteError (g.answerErrorRelative(), 0.0f, 1.0e-4f);
                expectEquals (g.answerErrorUnit(), juce::String ("ms"));
            }

            // A named pair: 0 or wrongError; no answer: missedError.
            {
                auto& g = gm.getGame (1);
                g.setDifficulty (5);
                g.newRound();
                expectEquals (g.answerErrorRelative(), Game::missedError);
                g.submitAnswer (g.getCorrectChoiceIndex());
                expectEquals (g.answerErrorRelative(), 0.0f);
                g.newRound();
                g.submitAnswer ((g.getCorrectChoiceIndex() + 1) % g.getNumChoices());
                expectEquals (g.answerErrorRelative(), Game::wrongError);
            }
        }

        beginTest ("bots: the scatter matches the chance of being right");
        {
            // P(|N(0, s)| <= 1) = erf (1 / (s sqrt 2)): one sigma holds 68.27 %.
            expectWithinAbsoluteError (RuleBasedPerceptualModel::sigmaForChance (0.6827f), 1.0f, 0.01f);
            expect (RuleBasedPerceptualModel::sigmaForChance (0.95f) < RuleBasedPerceptualModel::sigmaForChance (0.5f));
        }

        beginTest ("bots: the same seed, the same answers");
        {
            RuleBasedPerceptualModel cat (BotListener::Bot::cat);
            juce::Random a (42), b (42);
            for (int i = 0; i < 50; ++i)
            {
                const RoundContext round { i % 9, 1 + i % 10, i % 4, i % 2 == 0 };
                const auto x = cat.answer (round, a), y = cat.answer (round, b);
                expectEquals (x.relativeError, y.relativeError);
                expectEquals (x.thinkingMs, y.thinkingMs);
            }
        }

        beginTest ("bots: right as often on a ruler as their curve says");
        {
            RuleBasedPerceptualModel hound (BotListener::Bot::hound);
            juce::Random r (7);
            const RoundContext round { 0, 5, 3, true };
            int right = 0;
            constexpr int n = 6000;
            for (int i = 0; i < n; ++i)
                right += hound.answer (round, r).right ? 1 : 0;

            const auto expected = BotListener::chanceOfRight (BotListener::Bot::hound, 0, 5, 20, 3);
            expectWithinAbsoluteError ((float) right / n, expected, 0.04f);
        }

        beginTest ("bots differ by where they miss, not only by how often: the Cat up high, the Viper down low");
        {
            const auto meanError = [] (BotListener::Bot bot, int bucket)
            {
                RuleBasedPerceptualModel model (bot);
                juce::Random r (2026 + bucket);
                double sum = 0.0;
                constexpr int n = 5000;
                for (int i = 0; i < n; ++i)
                    sum += model.answer ({ 0, 6, bucket, true }, r).relativeError;
                return sum / n;
            };

            const auto catHigh = meanError (BotListener::Bot::cat, 6), viperHigh = meanError (BotListener::Bot::viper, 6);
            const auto catLow = meanError (BotListener::Bot::cat, 0), viperLow = meanError (BotListener::Bot::viper, 0);
            logMessage ("band, level 6, mean error in tolerances: Cat high " + juce::String (catHigh, 2) + ", Viper high " + juce::String (viperHigh, 2)
                        + "; Cat low " + juce::String (catLow, 2) + ", Viper low " + juce::String (viperLow, 2));

            expect (catHigh < viperHigh * 0.7, "the Cat hears the top better");
            expect (viperLow < catLow * 0.7, "the Viper hears the bottom better");
        }

        beginTest ("bots: a harder round, a bigger miss");
        {
            RuleBasedPerceptualModel owl (BotListener::Bot::owl);
            const auto mean = [&owl] (int level)
            {
                juce::Random r (99);
                double sum = 0.0;
                for (int i = 0; i < 4000; ++i)
                    sum += owl.answer ({ 3, level, 2, true }, r).relativeError;
                return sum / 4000.0;
            };
            expect (mean (2) < mean (9));
        }
    }
};

static BattleTest battleTest;

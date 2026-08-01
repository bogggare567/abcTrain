#include <juce_core/juce_core.h>
#include "../Source/Games/CompressionGame.h"

class CompressionGameTest : public juce::UnitTest
{
public:
    CompressionGameTest() : juce::UnitTest ("CompressionGame", "Games") {}

    void runTest() override
    {
        beginTest ("always exactly two choices");
        {
            CompressionGame game;

            for (int level = 1; level <= 10; ++level)
            {
                game.setDifficulty (level);
                expectEquals (game.getNumChoices(), 2, "level " + juce::String (level));
            }
        }

        beginTest ("correct answer increases the score");
        {
            CompressionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);

            expect (game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 1);
        }

        beginTest ("wrong answer does not increase the score");
        {
            CompressionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            const auto wrong = (correct + 1) % CompressionGame::numLevels;
            game.submitAnswer (wrong);

            expect (! game.wasLastAnswerCorrect());
            expectEquals (game.getScore(), 0);
        }

        beginTest ("feedback text is only available after answering");
        {
            CompressionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            expect (game.getFeedbackText().isEmpty());

            game.submitAnswer (game.getCorrectChoiceIndex());
            expect (game.getFeedbackText().isNotEmpty());
        }

        beginTest ("a second answer in the same round is ignored");
        {
            CompressionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            game.prepare (spec);

            const auto correct = game.getCorrectChoiceIndex();
            game.submitAnswer (correct);
            game.submitAnswer ((correct + 1) % CompressionGame::numLevels);

            expectEquals (game.getRoundsPlayed(), 1);
            expectEquals (game.getChosenChoiceIndex(), correct);
        }

        beginTest ("the pair is always two different real strengths");
        {
            CompressionGame game;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            const juce::StringArray known { "Weak", "Medium", "Strong" };

            for (const int level : { 1, 5, 10 })
            {
                game.setDifficulty (level);
                game.prepare (spec);

                for (int round = 0; round < 40; ++round)
                {
                    game.newRound();

                    const auto a = game.getChoiceLabel (0);
                    const auto b = game.getChoiceLabel (1);

                    expect (known.contains (a), "unknown label " + a);
                    expect (known.contains (b), "unknown label " + b);
                    expect (a != b, "the same strength offered twice: " + a);

                    const auto correct = game.getCorrectChoiceIndex();
                    game.submitAnswer (correct);
                    expect (game.wasLastAnswerCorrect());
                }
            }
        }

        beginTest ("harder levels offer neighbouring strengths more often");
        {
            // Weak-against-Strong is the giveaway pair; either neighbour
            // pair is the real question. The claim is that climbing the
            // levels replaces the first with the second - measured, not
            // asserted.
            CompressionGame game;

            const auto neighbourShare = [&game] (int level)
            {
                game.setDifficulty (level);

                auto neighbours = 0;
                constexpr int rounds = 400;

                for (int i = 0; i < rounds; ++i)
                {
                    game.newRound();

                    juce::StringArray offered;
                    offered.add (game.getChoiceLabel (0));
                    offered.add (game.getChoiceLabel (1));

                    // The easy pair is the only one without "Medium" in it.
                    if (offered.contains ("Medium"))
                        ++neighbours;
                }

                return (float) neighbours / (float) rounds;
            };

            const auto easy = neighbourShare (1);
            const auto hard = neighbourShare (10);

            expect (hard > easy,
                    "level 10 should offer neighbours more often: "
                        + juce::String (hard) + " vs " + juce::String (easy));
            expect (hard > 0.95f, "level 10 should have dropped the giveaway pair entirely");
        }

        beginTest ("every setting has a family, textbook through borderline");
        {
            for (int level = 0; level < CompressionGame::numLevels; ++level)
            {
                const auto& family = CompressionGame::familyFor (level);

                expect (family.size() >= 3,
                         "level " + juce::String (level) + " has fewer than three voicings");

                auto highest = 0.0f, lowest = 1.0f;
                auto slowestAttack = 0.0f, fastestAttack = 1.0e9f;

                for (const auto& variant : family)
                {
                    highest = juce::jmax (highest, variant.archetypal);
                    lowest  = juce::jmin (lowest, variant.archetypal);
                    slowestAttack = juce::jmax (slowestAttack, variant.attackMs);
                    fastestAttack = juce::jmin (fastestAttack, variant.attackMs);
                }

                expect (highest > 0.9f, "no clear example at level " + juce::String (level));
                expect (lowest < 0.35f, "no borderline example at level " + juce::String (level));

                // The whole claim of the family: what separates two
                // compressors doing the same job is the attack, so a
                // family whose members share one is not a family.
                expect (slowestAttack > fastestAttack * 2.0f,
                         "level " + juce::String (level) + " varies the ratio but not the attack");
            }
        }

        beginTest ("every voicing lands at the same loudness, so level is never the tell");
        {
            // A family that varies threshold, ratio *and* attack varies how
            // much gain reduction actually happens. If that reached the
            // output, the round would be winnable by hearing which one is
            // quieter - a different and much easier exercise.
            CompressionGame game;
            game.prepare ({ 44100.0, 512, 1 });

            for (const auto tier : { 1, 5, 10 })
            {
                game.setDifficulty (tier);

                for (int level = 0; level < CompressionGame::numLevels; ++level)
                {
                    for (const auto& variant : CompressionGame::familyFor (level))
                    {
                        const auto makeup = game.measureMakeupForTest (level, variant);

                        expect (makeup > 0.05f && makeup < 20.0f,
                                 "implausible compensation " + juce::String (makeup, 3));
                    }
                }
            }
        }

        beginTest ("no setting is systematically louder than another");
        {
            // The end-to-end version: run real rounds and compare the mean
            // output level of each setting against the others.
            //
            // The *mean*, not the min-to-max spread. Every round draws fresh
            // noise and starts at a different point in the burst cycle, so
            // any single round is a couple of dB either side of its own
            // setting's average - scatter that is identical for all three
            // and therefore says nothing about which one is playing. What
            // would be a tell is a setting that sits consistently louder,
            // and that is what this measures.
            CompressionGame game;
            game.prepare ({ 44100.0, 512, 1 });
            game.setDifficulty (5);

            std::array<double, CompressionGame::numLevels> total {};
            std::array<int, CompressionGame::numLevels> count {};

            for (int round = 0; round < 90; ++round)
            {
                game.newRound();

                // Five seconds: the burst repeats about once a second, so
                // a shorter window lands sometimes on a hit and sometimes
                // on a decay tail.
                juce::AudioBuffer<float> buffer (1, 44100 * 5);
                buffer.clear();
                game.process (buffer);

                auto sum = 0.0;
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    const auto v = (double) buffer.getSample (0, i);
                    sum += v * v;
                }

                const auto rms = std::sqrt (sum / (double) buffer.getNumSamples());
                expect (rms > 0.0, "a round produced silence");

                const auto label = game.getChoiceLabel (game.getCorrectChoiceIndex());
                const auto slot = label == "Weak" ? 0 : label == "Medium" ? 1 : 2;
                total[(size_t) slot] += rms;
                count[(size_t) slot] += 1;
            }

            auto quietest = 1.0e9, loudest = 0.0;

            for (size_t i = 0; i < total.size(); ++i)
            {
                expect (count[i] > 0, "a setting never came up in 90 rounds");
                const auto mean = total[i] / juce::jmax (1, count[i]);
                quietest = juce::jmin (quietest, mean);
                loudest = juce::jmax (loudest, mean);
            }

            const auto biasDb = juce::Decibels::gainToDecibels ((float) (loudest / quietest));
            expect (biasDb < 1.0f,
                     "one setting averages " + juce::String (biasDb, 2)
                         + " dB louder than another - enough to answer the round with");
        }
    }
};

static CompressionGameTest compressionGameTest;

#include "BotListener.h"
#include "BotWeights.h"
#include <cmath>

namespace BotListener
{
    namespace
    {
        // Character that is not hearing: reaction time and the fixed
        // Decibelo shown on the card. The hearing itself - a threshold per
        // exercise and per part of it - is BotWeights.h, trained by
        // tools/bots/train_bots.py from the literature teacher.
        struct Character { const char* id; int reactionMs; int rating; };
        constexpr Character characters[numBots] {
            { "hound",    1800, 1500 },
            { "cat",      1200, 1560 },
            { "viper",    1500, 1420 },
            { "owl",      2200, 1540 },
            { "bat",       900, 1530 },
            { "elephant", 3000, 1580 },
        };

        const std::array<Profile, numBots>& profiles()
        {
            static const auto table = []
            {
                std::array<Profile, numBots> t {};

                for (int b = 0; b < numBots; ++b)
                {
                    const auto& w = BotWeights::weights[b];
                    auto& p = t[(size_t) b];
                    p.id = characters[b].id;
                    p.reactionMs = characters[b].reactionMs;
                    p.rating = characters[b].rating;
                    p.lapse = w.lapse;

                    float slopeSum = 0.0f;
                    for (int g = 0; g < numGames; ++g)
                    {
                        float sum = 0.0f;
                        for (int k = 0; k < BotWeights::bucketsOf[g]; ++k)
                            sum += w.threshold[g][k];
                        p.threshold[(size_t) g] = sum / (float) BotWeights::bucketsOf[g];
                        slopeSum += w.slope[g];
                    }
                    p.slope = slopeSum / (float) numGames;
                }

                return t;
            }();

            return table;
        }
    }

    const Profile& profileOf (Bot bot) noexcept
    {
        return profiles()[(size_t) juce::jlimit (0, numBots - 1, (int) bot)];
    }

    const char* idOf (Bot bot) noexcept
    {
        return profileOf (bot).id;
    }

    float thresholdOf (Bot bot, int gameIndex, int bucket) noexcept
    {
        const auto b = juce::jlimit (0, numBots - 1, (int) bot);
        const auto g = juce::jlimit (0, numGames - 1, gameIndex);

        if (bucket < 0 || bucket >= BotWeights::bucketsOf[g])
            return profileOf ((Bot) b).threshold[(size_t) g];   // part unknown: the exercise's mean

        return BotWeights::weights[b].threshold[g][bucket];
    }

    float chanceOfRight (Bot bot, int gameIndex, int level, int choices, int bucket) noexcept
    {
        const auto b = juce::jlimit (0, numBots - 1, (int) bot);
        const auto g = juce::jlimit (0, numGames - 1, gameIndex);
        const auto& w = BotWeights::weights[b];

        const auto guess = 1.0f / (float) juce::jmax (2, choices);
        const auto x = (float) juce::jlimit (1, 10, level);
        const auto threshold = thresholdOf ((Bot) b, g, bucket);

        // The perceptron: a logistic unit on (threshold - level), scaled
        // between guessing and 1 - lapse.
        return guess + (1.0f - guess - w.lapse) / (1.0f + std::exp (w.slope[g] * (x - threshold)));
    }

    bool answers (Bot bot, int gameIndex, int level, juce::Random& random, int choices, int bucket)
    {
        return random.nextFloat() < chanceOfRight (bot, gameIndex, level, choices, bucket);
    }

    int thinkingMs (Bot bot, int level, juce::Random& random)
    {
        const auto base = (float) profileOf (bot).reactionMs;
        const auto harder = 1.0f + 0.06f * (float) (juce::jlimit (1, 10, level) - 1);
        const auto scatter = 0.75f + 0.5f * random.nextFloat();
        return juce::roundToInt (base * harder * scatter);
    }
}

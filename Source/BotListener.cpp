#include "BotListener.h"
#include <cmath>

namespace BotListener
{
    namespace
    {
        // Thresholds per exercise, in the exercise's own levels (1 easy,
        // 10 hardest). A player who has practised a few weeks sits around
        // 5-6; a bot's strong exercises sit above that, its weak ones below.
        //
        //                 band  comp  verb  pan  delay dist  width gain  range
        const Profile profiles[numBots] = {
            // Hound: the all-rounder - a little above average everywhere,
            // best on dynamics (level, envelope, transients).
            { "hound",    { 6.0f, 6.8f, 5.8f, 5.8f, 5.8f, 5.8f, 5.6f, 7.0f, 5.8f }, 1.1f, 0.05f, 1800, 1500 },

            // Cat: high frequencies, presence, air, narrow EQ, transients;
            // less sure of the lows (the range exercise's bottom octaves).
            { "cat",      { 7.6f, 6.4f, 5.0f, 5.2f, 6.2f, 6.4f, 5.0f, 5.4f, 5.2f }, 1.3f, 0.08f, 1200, 1560 },

            // Viper: lows, resonance, harmonics felt rather than heard -
            // distortion and the range exercise; weak on space and air.
            { "viper",    { 5.4f, 4.6f, 4.0f, 3.8f, 4.2f, 7.0f, 3.8f, 5.6f, 7.4f }, 1.0f, 0.10f, 1500, 1420 },

            // Owl: where a sound is - pan, width, reverb and delay.
            { "owl",      { 4.6f, 4.6f, 7.2f, 8.0f, 6.4f, 4.6f, 8.0f, 4.8f, 4.6f }, 1.2f, 0.06f, 2200, 1540 },

            // Bat: time - delay times and transients (compression attack),
            // and the top of the spectrum.
            { "bat",      { 6.2f, 7.2f, 5.4f, 5.0f, 8.4f, 4.8f, 5.0f, 5.0f, 4.8f }, 1.4f, 0.10f,  900, 1530 },

            // Elephant: slow and careful - lows, level and long changes
            // (gain, compression, reverb tails); rarely a careless miss.
            { "elephant", { 5.2f, 7.0f, 6.6f, 5.0f, 5.0f, 5.4f, 5.0f, 7.4f, 6.2f }, 0.9f, 0.03f, 3000, 1580 },
        };
    }

    const Profile& profileOf (Bot bot) noexcept
    {
        return profiles[juce::jlimit (0, numBots - 1, (int) bot)];
    }

    const char* idOf (Bot bot) noexcept
    {
        return profileOf (bot).id;
    }

    float chanceOfRight (Bot bot, int gameIndex, int level, int choices) noexcept
    {
        const auto& p = profileOf (bot);
        const auto threshold = p.threshold[(size_t) juce::jlimit (0, numGames - 1, gameIndex)];
        const auto guess = 1.0f / (float) juce::jmax (2, choices);
        const auto x = (float) juce::jlimit (1, 10, level);

        return guess + (1.0f - guess - p.lapse) / (1.0f + std::exp (p.slope * (x - threshold)));
    }

    bool answers (Bot bot, int gameIndex, int level, juce::Random& random, int choices)
    {
        return random.nextFloat() < chanceOfRight (bot, gameIndex, level, choices);
    }

    int thinkingMs (Bot bot, int level, juce::Random& random)
    {
        const auto base = (float) profileOf (bot).reactionMs;
        const auto harder = 1.0f + 0.06f * (float) (juce::jlimit (1, 10, level) - 1);
        const auto scatter = 0.75f + 0.5f * random.nextFloat();
        return juce::roundToInt (base * harder * scatter);
    }
}

#include "PerceptualModel.h"
#include "Games/Game.h"
#include <cmath>

float RuleBasedPerceptualModel::sigmaForChance (float p) noexcept
{
    // erf is monotonic: find x with erf(x) = p by bisection (24 steps is
    // far below a float's precision on [0, 4]), then sigma = 1 / (x sqrt 2).
    const auto target = juce::jlimit (0.02f, 0.999f, p);
    double lo = 0.0, hi = 4.0;

    for (int i = 0; i < 40; ++i)
    {
        const auto mid = 0.5 * (lo + hi);
        (std::erf (mid) < target ? lo : hi) = mid;
    }

    return (float) (1.0 / (0.5 * (lo + hi) * std::sqrt (2.0)));
}

BotAnswer RuleBasedPerceptualModel::answer (const RoundContext& round, juce::Random& random) const
{
    BotAnswer a;
    a.thinkingMs = BotListener::thinkingMs (bot, round.level, random);

    const auto& profile = BotListener::profileOf (bot);

    if (! round.continuous)
    {
        a.right = BotListener::answers (bot, round.gameIndex, round.level, random, 2, round.bucket);
        a.relativeError = a.right ? 0.0f : Game::wrongError;
        return a;
    }

    // A stray tap: anywhere well outside.
    if (random.nextFloat() < profile.lapse)
    {
        a.relativeError = 1.5f + 2.5f * random.nextFloat();
        a.right = false;
        return a;
    }

    // The chance of a ruler answer inside the tolerance, without the lapse
    // (drawn above) and without a guessing floor (a ruler has none to speak
    // of: "choices" 20 makes it 5%).
    const auto p = BotListener::chanceOfRight (bot, round.gameIndex, round.level, 20, round.bucket);
    const auto core = juce::jlimit (0.02f, 0.999f, p / juce::jmax (0.5f, 1.0f - profile.lapse));
    const auto sigma = sigmaForChance (core);

    // |N(0, sigma)| by Box-Muller.
    const auto u1 = juce::jmax (1.0e-7f, random.nextFloat());
    const auto u2 = random.nextFloat();
    const auto gauss = std::sqrt (-2.0f * std::log (u1)) * std::cos (juce::MathConstants<float>::twoPi * u2);

    // A ruler ends somewhere: no answer is more than a few tolerances off
    // that still means anything (and damage has long saturated there).
    a.relativeError = juce::jmin (6.0f, std::abs (gauss) * sigma);
    a.right = a.relativeError <= 1.0f;
    return a;
}

std::unique_ptr<PerceptualModel> makePerceptualModel (BotListener::Bot bot)
{
    return std::make_unique<RuleBasedPerceptualModel> (bot);
}

#pragma once

#include "BotListener.h"
#include <memory>

// How a virtual listener answers a battle round (ADR 049).
//
// BotListener (ADR 046) says how likely a bot is to be *right*. A battle
// with damage needs more: *how far off* it is - and two bots that are right
// equally often can still miss differently (one by a hair, one wildly).
// So a model returns an error on the same scale the player's answer is
// measured on: Game::answerErrorRelative - the miss divided by the round's
// tolerance, 0 dead on, 1 at the edge of "right".
//
// One interface, so the rules here can later be swapped for a model trained
// on real players' answers (exercise, level, target, error, reaction time,
// recent form) - a PerceptronPerceptualModel - without touching the battle.
struct RoundContext
{
    int gameIndex = 0;
    int level = 1;
    int bucket = -1;          // Game::getSkillBucketForRound(), -1 if unknown
    bool continuous = false;  // a ruler (distance) or a named pair (right/wrong)
};

struct BotAnswer
{
    float relativeError = 0.0f;   // as Game::answerErrorRelative
    bool right = false;           // relativeError <= 1
    int thinkingMs = 0;           // for pacing the reveal, not for scoring
};

class PerceptualModel
{
public:
    virtual ~PerceptualModel() = default;
    virtual BotAnswer answer (const RoundContext&, juce::Random&) const = 0;
};

// The rule-based model: the bot's psychometric curve (BotListener) turned
// into an error distribution.
//
// On a ruler the bot aims at the target with Gaussian scatter. The scatter
// is chosen so the chance of landing inside the tolerance equals the bot's
// chanceOfRight for that exercise, part and level:
//
//     P(|e| <= 1) = erf (1 / (sigma * sqrt 2))  =  p    =>  sigma = 1 / (sqrt 2 * erfinv p)
//
// so the Cat, sure of 8 kHz, misses there by a few hundredths of an octave,
// and the Viper by half an octave - from the same trained weights the bot
// battles used before, not from new numbers. A lapse (a stray tap) lands
// anywhere: 1.5 to 4 tolerances off. On a named pair there is no distance:
// right (0) or wrong (Game::wrongError), drawn from the same chance.
class RuleBasedPerceptualModel : public PerceptualModel
{
public:
    explicit RuleBasedPerceptualModel (BotListener::Bot b) : bot (b) {}
    BotAnswer answer (const RoundContext&, juce::Random&) const override;

    // The scatter on a ruler, in tolerances, for a chance p of landing inside.
    static float sigmaForChance (float p) noexcept;

    BotListener::Bot getBot() const noexcept { return bot; }

private:
    BotListener::Bot bot;
};

std::unique_ptr<PerceptualModel> makePerceptualModel (BotListener::Bot);

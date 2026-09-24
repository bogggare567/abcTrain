#pragma once

#include <juce_core/juce_core.h>
#include <array>

// Virtual listeners to battle when there is nobody to battle (ADR 046):
// a Cat, a Hound, a Viper, an Owl, a Bat and an Elephant.
//
// The idea is the owner's; the shape is from the review in notes/ ("each bot
// a model of a hearing profile, not a filter"). Each bot is a
// psychometric function per exercise - the probability of answering right at
// each of the exercise's ten levels:
//
//     P(correct | level) = guess + (1 - guess - lapse) / (1 + exp(slope * (level - threshold)))
//
// the standard shape of a listener's detection curve. `threshold` is where it
// gets half-way between guessing and certain; `lapse` the rate of answering
// wrong on an easy round anyway (attention, a stray tap); `slope` how sharply
// it goes from hearing it to not. So a bot is not random: it is good where
// its profile is good, gets worse as the round gets harder exactly like the
// player does, and loses sometimes on easy rounds - like people.
//
// The animals are characters, not biology. "Cat - high-frequency
// specialist" is a game profile inspired by documented differences in animal
// hearing, not a simulation of a cat's ear, and the app says so. A spider
// was suggested too; it waits for an exercise about vibration.
//
// Pure: no GUI, no audio, no message loop - tests drive it with a seeded
// Random.
namespace BotListener
{
    enum class Bot { hound, cat, viper, owl, bat, elephant };
    constexpr int numBots = 6;

    // Exercises in GameManager order (append-only, so these indices hold):
    // 0 band, 1 compression, 2 reverb, 3 pan, 4 delay, 5 distortion,
    // 6 stereo width, 7 gain change, 8 range.
    constexpr int numGames = 9;

    struct Profile
    {
        const char* id;            // stable, for settings and i18n keys: bots.<id>.name
        std::array<float, numGames> threshold;   // level 1..10 where it is half-way
        float slope;               // per level
        float lapse;               // wrong on an easy round anyway
        int reactionMs;            // typical time to answer
        int rating;                // its Decibelo, for the list (fixed: bots do not climb)
    };

    const Profile& profileOf (Bot) noexcept;
    const char* idOf (Bot) noexcept;

    // The chance of a right answer on exercise `gameIndex` at `level`
    // (1..10). `choices`: how many answers the round offers - two for a
    // named pair, more for a ruler's zones - which sets the guessing floor.
    float chanceOfRight (Bot, int gameIndex, int level, int choices = 2) noexcept;

    // One round: right or wrong, drawn from chanceOfRight.
    bool answers (Bot, int gameIndex, int level, juce::Random&, int choices = 2);

    // How long it "thinks": the profile's reaction time, longer on harder
    // rounds, with some scatter. For pacing the reveal, not for scoring.
    int thinkingMs (Bot, int level, juce::Random&);
}

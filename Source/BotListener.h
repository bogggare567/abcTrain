#pragma once

#include <juce_core/juce_core.h>
#include <array>

// Virtual listeners to battle when there is nobody to battle (ADR 046):
// a Cat, a Hound, a Viper, an Owl, a Bat and an Elephant.
//
// The idea is the owner's; the shape is from the review in notes/ ("each bot
// a model of a hearing profile, not a filter"). Each bot is a one-neuron
// perceptron per exercise with a logistic output - the psychometric function:
//
//     P(correct | bucket, level) = guess + (1 - guess - lapse) / (1 + exp(slope * (level - threshold[bucket])))
//
// `bucket` is the part of the exercise the round fell in (the frequency
// range, the pan zone, the reverb type - Game::getSkillBucketForRound), so
// the Cat is sure of a boost at 8 kHz and guesses at 35 Hz on the same level.
// The weights (BotWeights.h) are trained by tools/bots/train_bots.py on
// rounds simulated from published animal hearing data
// (docs/research/2026-09-bot-hearing.md); the same script fits real answers.
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
        const char* id = "";       // stable, for settings and i18n keys: bots.<id>.name
        std::array<float, numGames> threshold {};   // per exercise, mean over its parts (level 1..10, half-way)
        float slope = 1.0f;        // mean over exercises, per level
        float lapse = 0.05f;       // wrong on an easy round anyway
        int reactionMs = 1500;     // typical time to answer
        int rating = 1500;         // its Decibelo, for the list (fixed: bots do not climb)
    };

    const Profile& profileOf (Bot) noexcept;
    const char* idOf (Bot) noexcept;

    // The chance of a right answer on exercise `gameIndex` at `level`
    // (1..10). `choices`: how many answers the round offers - two for a
    // named pair, more for a ruler's zones - which sets the guessing floor.
    // `bucket`: Game::getSkillBucketForRound(), or -1 for "not known" (the
    // exercise's mean threshold).
    float chanceOfRight (Bot, int gameIndex, int level, int choices = 2, int bucket = -1) noexcept;

    // The level where the bot is half-way, for one part of an exercise.
    float thresholdOf (Bot, int gameIndex, int bucket) noexcept;

    // One round: right or wrong, drawn from chanceOfRight.
    bool answers (Bot, int gameIndex, int level, juce::Random&, int choices = 2, int bucket = -1);

    // How long it "thinks": the profile's reaction time, longer on harder
    // rounds, with some scatter. For pacing the reveal, not for scoring.
    int thinkingMs (Bot, int level, juce::Random&);
}

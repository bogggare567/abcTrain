// RoundOracle: the battle server's judge (b05, ADR 056).
//
// A battle round is built in every app from (exercise, level, seed); until
// now the server learned the right answer only from what the apps claimed,
// so in a duel a modified app could void any round it lost by claiming its
// own answer was the right one. This is the same game engine with no
// window and no sound card: the server keeps one running and asks it, the
// moment a round is created, what that round's right answer is. The apps
// then send only what they chose; the answer goes back only after the round.
//
// Protocol, one line each way:
//   in:  <exercise index> <level> <seed>
//   out: {"continuous":false,"correct":1}
//        {"continuous":true,"correctNorm":0.4321,"tolerance":0.0625}
//        {"error":"..."}
// About 1 ms a question after a ~0.2 s start.

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>
#include "../Source/GameManager.h"
#include <iostream>
#include <string>

static juce::String answerFor (GameManager& manager, int exercise, int level, juce::int64 seed)
{
    if (exercise < 0 || exercise >= manager.getNumGames())
        return R"({"error":"exercise"})";

    auto& game = manager.getGame (exercise);
    game.setSeededRounds (true);
    game.setDifficulty (juce::jlimit (1, 10, level));
    game.seedNextRound (seed);
    game.newRound();

    if (game.usesContinuousScale())
        return "{\"continuous\":true,\"correctNorm\":" + juce::String (game.getCorrectNormalised(), 6)
             + ",\"tolerance\":" + juce::String (game.getToleranceNormalised(), 6) + "}";

    return "{\"continuous\":false,\"correct\":" + juce::String (game.getCorrectChoiceIndex()) + "}";
}

int main()
{
    // A message manager for the games' ChangeBroadcasters; nothing is ever
    // dispatched, nothing needs to be.
    juce::MessageManager::getInstance();

    // One engine for the life of the process, like the app's: a seeded
    // round depends on (exercise, level, seed) only - the apps reuse their
    // game too, so if history leaked in they would already disagree with
    // each other (tests/RoundOracleTest holds this).
    GameManager manager;
    manager.prepare ({ 48000.0, 512, 2 });

    std::string line;
    while (std::getline (std::cin, line))
    {
        const auto parts = juce::StringArray::fromTokens (juce::String (line), " ", "");
        if (parts.size() != 3)
        {
            std::cout << R"({"error":"usage: exercise level seed"})" << std::endl;
            continue;
        }

        std::cout << answerFor (manager, parts[0].getIntValue(), parts[1].getIntValue(), parts[2].getLargeIntValue())
                  << std::endl;
    }
    juce::MessageManager::deleteInstance();
    return 0;
}

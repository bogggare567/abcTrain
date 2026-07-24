#pragma once

#include "Games/Game.h"
#include <juce_dsp/juce_dsp.h>

// Owns every registered exercise and tracks which one is active. This is
// the processor's only touchpoint into the games - adding a new exercise
// means registering it in the constructor, no processor/editor changes.
class GameManager
{
public:
    GameManager();

    // Prepares every registered game up front (not just the active one),
    // so switching games never needs a re-prepare on the audio thread.
    void prepare (const juce::dsp::ProcessSpec&);
    void process (juce::AudioBuffer<float>&);

    void setActiveGameIndex (int index);
    int getActiveGameIndex() const noexcept { return activeIndex; }

    Game& getActiveGame() noexcept { return *games[activeIndex]; }
    juce::StringArray getGameNames() const;

    // For ProgressManager, which needs to listen to every game (not just
    // the active one) and apply difficulty to all of them.
    int getNumGames() const noexcept { return games.size(); }
    Game& getGame (int index) noexcept { return *games[index]; }

    void setDifficultyForAllGames (int level);

private:
    juce::OwnedArray<Game> games;
    int activeIndex = 0;
};

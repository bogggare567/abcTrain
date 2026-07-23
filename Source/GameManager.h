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

private:
    juce::OwnedArray<Game> games;
    int activeIndex = 0;
};

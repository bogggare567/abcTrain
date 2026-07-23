#include "GameManager.h"
#include "Games/EQGame.h"
#include "Games/CompressionGame.h"

GameManager::GameManager()
{
    games.add (new EQGame());
    games.add (new CompressionGame());
}

void GameManager::prepare (const juce::dsp::ProcessSpec& spec)
{
    for (auto* game : games)
        game->prepare (spec);
}

void GameManager::process (juce::AudioBuffer<float>& buffer)
{
    getActiveGame().process (buffer);
}

void GameManager::setActiveGameIndex (int index)
{
    if (index < 0 || index >= games.size())
        return;

    activeIndex = index;
}

juce::StringArray GameManager::getGameNames() const
{
    juce::StringArray names;
    for (auto* game : games)
        names.add (game->getName());
    return names;
}

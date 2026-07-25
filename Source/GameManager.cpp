#include "GameManager.h"
#include "Games/EQGame.h"
#include "Games/CompressionGame.h"
#include "Games/ReverbGame.h"
#include "Games/PanGame.h"
#include "Games/DelayGame.h"
#include "Games/DistortionGame.h"
#include "Games/StereoWidthGame.h"
#include "Games/DBGame.h"
#include "Games/FrequencyRangeGame.h"

GameManager::GameManager()
{
    games.add (new EQGame());
    games.add (new CompressionGame());
    games.add (new ReverbGame());
    games.add (new PanGame());
    games.add (new DelayGame());
    games.add (new DistortionGame());
    games.add (new StereoWidthGame());
    games.add (new DBGame());
    games.add (new FrequencyRangeGame());

    // Every game's default no-op override (see Game::setReferenceAudioLibrary)
    // means this is harmless even for StereoWidthGame, which deliberately
    // ignores it.
    for (auto* game : games)
        game->setReferenceAudioLibrary (&referenceAudioLibrary);
}

void GameManager::prepare (const juce::dsp::ProcessSpec& spec)
{
    referenceAudioLibrary.prepare (spec.sampleRate);

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

void GameManager::setDifficultyForAllGames (int level)
{
    for (auto* game : games)
        game->setDifficulty (level);
}

#pragma once

#include "Games/Game.h"
#include "../shared/ReferenceAudioLibrary.h"
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
    // Also (re)loads whatever reference-audio selection was persisted
    // from a previous session, now that the real sample rate is known -
    // see ReferenceAudioLibrary::prepare().
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

    // Lets a player practice on real reference audio instead of pink
    // noise - see ReferenceAudioLibrary/TestSignalGenerator. Shared by
    // every registered game (each was wired to it via
    // setReferenceAudioLibrary() in the constructor below); the editor's
    // Training Sounds screen is the only thing that ever calls into this.
    ReferenceAudioLibrary& getReferenceAudioLibrary() noexcept { return referenceAudioLibrary; }

private:
    // Declaration order matters: referenceAudioProperties must be
    // constructed before referenceAudioLibrary, which holds a reference
    // to it (same pattern as LocalisationManager/its PropertiesFile).
    juce::PropertiesFile referenceAudioProperties { ReferenceAudioLibrary::makeDefaultOptions() };
    ReferenceAudioLibrary referenceAudioLibrary { referenceAudioProperties };

    juce::OwnedArray<Game> games;
    int activeIndex = 0;
};

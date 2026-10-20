#pragma once

#include "Games/Game.h"
#include "shared/audio/ReferenceAudioLibrary.h"
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

    // A battle round, built from what the server sent: the seed's built-in
    // clip first, then the round. That order matters since the frequency
    // exercises draw only where the clip has sound (SpectralContent.h) -
    // drawing first and loading the clip after asked about a sound that
    // was not yet the one playing. One function for the app, the server's
    // judge (tools/RoundOracle) and tools/BattleSmoke, so the three cannot
    // build the same seed two ways. Message thread.
    void startSeededRound (Game& game, int level, juce::int64 seed, double sampleRate)
    {
        startSeededRound (referenceAudioLibrary, game, level, seed, sampleRate);
    }

    // The same, against any library - the tests use one with throwaway
    // settings rather than the player's own.
    static void startSeededRound (ReferenceAudioLibrary& library, Game& game, int level,
                                  juce::int64 seed, double sampleRate)
    {
        // Another number from the same seed for the clip, so the sound and
        // the setting are not tied to each other round after round.
        library.selectSeededBuiltIn (seed * 2654435761LL + 17, sampleRate);

        game.setSeededRounds (true);
        game.setDifficulty (level);
        game.seedNextRound (seed);
        game.newRound();
    }

    // When no sound is selected: each exercise's own synthesized material
    // (true) or pink noise (false). Persisted by the library, applied to
    // every game. See Game::setPreferExerciseSound and ADR 040.
    void setPreferExerciseSound (bool shouldPrefer);

private:
    // Declaration order matters: referenceAudioProperties must be
    // constructed before referenceAudioLibrary, which holds a reference
    // to it (same pattern as LocalisationManager/its PropertiesFile).
    juce::PropertiesFile referenceAudioProperties { ReferenceAudioLibrary::makeDefaultOptions() };
    ReferenceAudioLibrary referenceAudioLibrary { referenceAudioProperties };

    juce::OwnedArray<Game> games;
    int activeIndex = 0;
};

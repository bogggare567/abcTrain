#pragma once

#include "LocalRoom.h"
#include "Games/Game.h"
#include <functional>

// Runs a local seminar: the exercise plays on this computer into the hall,
// the room (LocalRoom) collects the audience's answers from their phones,
// the projector window shows the question, the votes and the answer.
//
// The host drives it: Start, then for every round "Show the answer" and
// "Next". The presenter never answers the exercise himself - his progress
// and records are not touched by a seminar: only newRound() is called on
// the game, and ProgressManager ignores that (it scores answers).
//
// Families are the four the rest of Live uses (frequency, dynamics, space,
// character); each round takes the next chosen family and an exercise of
// it at random, so a ten-round seminar with two families alternates them.
class SeminarHost
{
public:
    // What the host needs from the app. All message thread.
    struct Hooks
    {
        std::function<int()> activeGameIndex;
        std::function<Game&(int)> game;
        std::function<void (int)> selectGame;          // switches the app's active exercise
        std::function<void()> startRound;              // newRound() + a fresh clip, clean first
        std::function<void (bool play, bool processed)> setSound;
        std::function<juce::String (const Game&)> localisedName;
        std::function<juce::String (const Game&)> localisedPrompt;
    };

    explicit SeminarHost (Hooks);

    LocalRoom& getRoom() noexcept { return room; }

    // families: bit 0 frequency ... bit 3 character.
    void configure (const juce::String& title, int familyMask, int rounds,
                    bool listOnly, std::vector<LocalRoom::Invitee>);

    bool openRoom (juce::String& error);
    void closeRoom();

    enum class Stage { closed, lobby, listening, revealed, finished };
    Stage getStage() const noexcept { return stage; }
    int getRound() const noexcept { return round; }
    int getTotalRounds() const noexcept { return totalRounds; }

    void start();               // lobby -> round 1
    void reveal();              // listening -> revealed
    void next();                // revealed -> next round, or finished after the last
    void again();               // finished -> lobby, scores reset, same people

    // The hall's sound: off, or the clean / processed version.
    void setSound (bool play, bool processed);
    bool isPlaying() const noexcept { return playing; }
    bool isProcessed() const noexcept { return processed; }

    // The exercise indices of a family (0 frequency ... 3 character) - the
    // same split the bot battles use.
    static const std::vector<int>& gamesOfFamily (int family);

private:
    void askCurrent();

    Hooks hooks;
    LocalRoom room;
    Stage stage = Stage::closed;
    int familyMask = 1, totalRounds = 10, round = 0;
    bool playing = false, processed = false;
    juce::Random random;
};

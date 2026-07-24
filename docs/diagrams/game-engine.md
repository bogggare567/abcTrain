# EarTrainer game engine

Every exercise implements one interface, so `GameManager` and
`PluginEditor` never need per-game special cases. See
[decisions/001-game-interface.md](../decisions/001-game-interface.md) for
why this shape was chosen over a per-game editor, and
[decisions/002-difficulty-scaling.md](../decisions/002-difficulty-scaling.md)
for why `setDifficulty(int)` was added and how `ProgressManager` drives it.

```mermaid
classDiagram
    class Game {
        <<interface>>
        +getName() String
        +getInstructions() String
        +prepare(ProcessSpec spec)
        +process(AudioBuffer buffer)
        +setDifficulty(int level)
        +newRound()
        +submitAnswer(int choiceIndex)
        +getNumChoices() int
        +getChoiceLabel(int choiceIndex) String
        +hasAnswered() bool
        +getCorrectChoiceIndex() int
        +getChosenChoiceIndex() int
        +wasLastAnswerCorrect() bool
        +getFeedbackText() String
        +getScore() int
        +getRoundsPlayed() int
    }

    class EQGame {
        8 octave bands, 100 Hz-12.8 kHz
        peak filter, random 9 dB boost/cut
    }

    class CompressionGame {
        3 presets: weak/medium/strong
        noise burst through dsp::Compressor
        loudness-matched makeup gain
    }

    class ReverbGame {
        4 types: Room, Hall, Plate, Spring
        Room/Hall/Plate: dsp::Reverb presets
        Spring: cascaded allpass filters
    }

    Game <|.. EQGame
    Game <|.. CompressionGame
    Game <|.. ReverbGame

    class GameManager {
        -games OwnedArray~Game~
        -activeIndex int
        +prepare(ProcessSpec spec)
        +process(AudioBuffer buffer)
        +setActiveGameIndex(int index)
        +getActiveGame() Game
        +getGame(int index) Game
        +getGameNames() StringArray
        +setDifficultyForAllGames(int level)
    }

    GameManager o-- Game : owns all, prepares all up front,\ndelegates process() to the active one

    class ProgressManager {
        -totalScore int
        -level int
        -streakDays int
        -dailyChallengeGameIndex int
        +registerAnswer(int gameIndex, bool wasCorrect)
        +getLevel() int
        +getDailyChallengeDescription() String
    }

    ProgressManager --> GameManager : listens to every game via\nChangeListener, calls\nsetDifficultyForAllGames() on level-up

    class PluginProcessor {
        +prepareToPlay()
        +processBlock()
    }

    class PluginEditor {
        -choiceButtons OwnedArray~TextButton~
        -gameSelector ComboBox
        -levelProgressBar LevelProgressBar
        +rebuildChoiceButtons()
        +refreshFromGameState()
        +refreshFromProgressState()
    }

    PluginProcessor --> GameManager
    PluginProcessor --> ProgressManager
    PluginEditor --> GameManager : reads active game's\ngetNumChoices() / getChoiceLabel()\nto build UI, listens for ChangeBroadcaster
    PluginEditor --> ProgressManager : reads level/streak/daily\nchallenge, listens for ChangeBroadcaster
```

**Why `GameManager` prepares every game up front:** switching the active
game (via the editor's `ComboBox`) then never needs an audio-thread
re-prepare — every registered `Game` is always ready, regardless of which
one is currently selected.

**Why `ProgressManager` calls `registerAnswer` instead of always going
through the `ChangeListener` chain:** `Game::sendChangeMessage()` is
asynchronous (needs a running JUCE message loop to deliver) — fine in the
real plugin, but `EarTrainerTests` never runs one, so
`ProgressManagerTest` drives `registerAnswer` directly instead. See
[testing-strategy.md](../testing-strategy.md).

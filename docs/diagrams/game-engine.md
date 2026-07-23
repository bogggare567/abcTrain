# EarTrainer game engine

Every exercise implements one interface, so `GameManager` and
`PluginEditor` never need per-game special cases. See
[decisions/001-game-interface.md](../decisions/001-game-interface.md) for
why this shape was chosen over a per-game editor.

```mermaid
classDiagram
    class Game {
        <<interface>>
        +getName() String
        +getInstructions() String
        +prepare(ProcessSpec spec)
        +process(AudioBuffer buffer)
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
        <<planned>>
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
        +getGameNames() StringArray
    }

    GameManager o-- Game : owns all, prepares all up front,\ndelegates process() to the active one

    class PluginProcessor {
        +prepareToPlay()
        +processBlock()
    }

    class PluginEditor {
        -choiceButtons OwnedArray~TextButton~
        -gameSelector ComboBox
        +rebuildChoiceButtons()
        +refreshFromGameState()
    }

    PluginProcessor --> GameManager
    PluginEditor --> GameManager : reads active game's\ngetNumChoices() / getChoiceLabel()\nto build UI, listens for ChangeBroadcaster
```

**Why `GameManager` prepares every game up front:** switching the active
game (via the editor's `ComboBox`) then never needs an audio-thread
re-prepare — every registered `Game` is always ready, regardless of which
one is currently selected.

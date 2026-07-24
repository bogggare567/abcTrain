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

    class PanGame {
        5 positions: Hard Left..Hard Right
        equal-power pan law
        loudness-equalized for free
    }

    class DelayGame {
        4 fixed times: 50/150/300/500 ms
        dsp::DelayLine, feedback=0, 50% wet
        difficulty scales burst period, not the times
    }

    class DistortionGame {
        4 types: Soft/Hard Clip, Tape, Overdrive
        tanh / hard clip / tanh+lowpass / asymmetric tanh
        difficulty scales drive amount
    }

    class StereoWidthGame {
        4 widths: Narrow..Extra Wide
        two independent PinkNoiseGenerators, M/S width
        needs decorrelated L/R, unlike every other game
    }

    class DBGame {
        5 deltas: labels change with difficulty
        step size 6/3/2 dB per tier
        only game whose choice labels aren't fixed
    }

    Game <|.. EQGame
    Game <|.. CompressionGame
    Game <|.. ReverbGame
    Game <|.. PanGame
    Game <|.. DelayGame
    Game <|.. DistortionGame
    Game <|.. StereoWidthGame
    Game <|.. DBGame

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

**How the five newer games' `setDifficulty` differs from
`CompressionGame`'s "same labels, converging values" precedent:**
`PanGame`/`StereoWidthGame` follow that precedent exactly (same
qualitative labels at every tier, underlying values converge toward
"normal" as it gets harder). `DelayGame`/`DistortionGame` keep their
choice set fixed too, but scale a *different* DSP parameter behind the
scenes (burst period, drive amount) rather than the values the labels
describe, since those specific labels (delay times, distortion type
names) were spec'd as literal, non-tiered choices. `DBGame` is the one
exception to "labels never change": since its labels *are* the dB
numbers, there's no way to keep them fixed while making the underlying
gap smaller, so its labels are recomputed from the current tier's step
size instead.

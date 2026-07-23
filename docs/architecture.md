# Architecture: multi-game ear trainer

> This is the original design doc written before the `Game`/`GameManager`
> refactor, kept as-is for the rationale. It describes the *plan* — the
> code was built to match it closely, with one deliberate deviation noted
> below. For diagrams, current status, and the ADR, see:
>
> - [diagrams/system-overview.md](diagrams/system-overview.md) — whole repo
> - [diagrams/game-engine.md](diagrams/game-engine.md) — this `Game`/`GameManager` design, as built
> - [diagrams/learner-plugin.md](diagrams/learner-plugin.md) — LearnerEQ, a different shape of plugin this doc doesn't cover
> - [decisions/001-game-interface.md](decisions/001-game-interface.md) — why this interface shape was chosen over the alternatives
> - [roadmap.md](roadmap.md) — what's done vs. planned
> - [testing-strategy.md](testing-strategy.md)
>
> **Deviation from this doc:** the "`GameManager` re-prepares the newly
> active game on switch" design below wasn't built. The implementation
> instead prepares *every* registered game up front in `prepare()`, so
> switching games never needs an audio-thread re-prepare at all — simpler
> than what's described here, and it fully replaces the need for it.

## Problem with the current code

`EQGame` currently owns both the DSP/state logic (filter, scoring, random
round generation) *and* is wired one-to-one into `PluginEditor`, which
hardcodes 8 frequency-band buttons. Adding a second exercise (compression
strength, reverb type, ...) would mean either duplicating the editor for
each game or growing it into a pile of per-game special cases.

## Key observation

Every planned exercise in the roadmap (EQ band, compression strength,
reverb type, delay type, phaser-vs-chorus, stereo width) has the *same*
shape: play a processed test signal, offer a fixed set of labeled choices,
the player picks one, get right/wrong feedback + a running score. Only two
things vary per game: **how the audio is generated/processed**, and
**what the choices are called**. That means one generic multiple-choice UI
can drive every game — no per-game editor subclass needed.

## `Game` interface (`Source/Games/Game.h`)

```cpp
class Game : public juce::ChangeBroadcaster
{
public:
    virtual ~Game() = default;

    virtual juce::String getName() const = 0;
    virtual juce::String getInstructions() const = 0;

    virtual void prepare (const juce::dsp::ProcessSpec&) = 0;
    virtual void process (juce::AudioBuffer<float>&) = 0;

    virtual void newRound() = 0;
    virtual void submitAnswer (int choiceIndex) = 0;

    virtual int getNumChoices() const = 0;
    virtual juce::String getChoiceLabel (int choiceIndex) const = 0;

    virtual bool hasAnswered() const = 0;
    virtual int getCorrectChoiceIndex() const = 0;
    virtual int getChosenChoiceIndex() const = 0;
    virtual juce::String getFeedbackText() const = 0;

    virtual int getScore() const = 0;
    virtual int getRoundsPlayed() const = 0;
};
```

`submitAnswer`/`getCorrectChoiceIndex`/etc. are deliberately index-based
(not enums per game) so the generic UI never needs to know what a "choice"
means for a given game — it just renders `getNumChoices()` buttons labeled
with `getChoiceLabel(i)`.

Each concrete game (`EQGame`, `CompressionGame`, ...) lives in
`Source/Games/` and implements this interface, keeping its own DSP chain
and randomization private.

## `GameManager` (`Source/GameManager.h`/`.cpp`)

Owns every registered `Game` (`juce::OwnedArray<Game>`), tracks which one
is active, and is the processor's only touchpoint:

```cpp
class GameManager
{
public:
    GameManager();               // registers EQGame, CompressionGame, ...

    void prepare (const juce::dsp::ProcessSpec&);
    void process (juce::AudioBuffer<float>&);   // delegates to active game

    void setActiveGameIndex (int index);        // re-prepares the newly active game
    int getActiveGameIndex() const;
    Game& getActiveGame();

    juce::StringArray getGameNames() const;
};
```

`prepare()` caches the `ProcessSpec` so that switching games mid-session
can re-prepare the newly active one with the right sample rate/block size
without the processor needing to know about that detail.

## Processor changes

`PluginProcessor` replaces its `EQGame game` member with `GameManager
gameManager`. `prepareToPlay` calls `gameManager.prepare(spec)`,
`processBlock` calls `gameManager.process(buffer)`. No other processor
logic changes.

## Editor changes

`PluginEditor` becomes generic:

- A `juce::ComboBox` listing `gameManager.getGameNames()` to switch games
  (calls `setActiveGameIndex`, then rebuilds the choice buttons).
- Instructions label bound to `getActiveGame().getInstructions()`.
- A `juce::OwnedArray<juce::TextButton>` rebuilt to size
  `getNumChoices()` whenever the active game changes (frequency bands = 8
  buttons, compression strength = 3 buttons, etc.) — button `i` calls
  `submitAnswer(i)`.
- Feedback/score labels read `getFeedbackText()`/`getScore()`/
  `getRoundsPlayed()` from whichever game is active.
- Editor listens to the *active* `Game`'s change broadcasts (re-subscribes
  on switch) — no per-game editor code at all.

## Adding a new game later

1. Create `Source/Games/NewGame.{h,cpp}` implementing `Game`.
2. Register it in `GameManager`'s constructor.
3. Add the two new source files to `CMakeLists.txt`.

No editor or processor changes required — this is the "open/closed" part.

## Refactor + first new game plan

1. Add `Source/Games/Game.h` (interface above).
2. Move `EQGame` into `Source/Games/EQGame.{h,cpp}`, adapt it to the
   `Game` interface (mechanically: band index *is* the choice index,
   `getChoiceLabel` formats the frequency, `getFeedbackText` builds the
   same "Correct/Not quite, it was boosted/cut at X Hz" string it already
   builds today).
3. Add `Source/GameManager.{h,cpp}`.
4. Rewrite `PluginProcessor` to own `GameManager` instead of `EQGame`.
5. Rewrite `PluginEditor` to be generic (game selector + dynamic choice
   buttons), replacing the hardcoded 8-band layout.
6. Add `Source/Games/CompressionGame.{h,cpp}`: a 3-choice "how strong is
   this compression?" exercise. Test signal: a repeating percussive noise
   burst (fast attack, exponential decay, ~2 Hz) so gain reduction is
   audible as pumping, not just loudness. Three fixed presets (threshold/
   ratio) for weak/medium/strong, each processed through
   `juce::dsp::Compressor` with a **loudness-matched makeup gain** so the
   player is judging compression character, not raw volume.
7. Update `CLAUDE.md` to describe the new architecture, replacing the
   "no GameManager yet" note.

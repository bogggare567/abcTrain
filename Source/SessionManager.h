#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "BotListener.h"

// How a training run is framed: unlimited, life-limited, or time-limited.
//
// Until now every exercise ran the same way forever - answer, press "New
// Round", answer again, with nothing at stake and nothing to finish. These
// three modes give a run a shape:
//
//  - practice: no lives, no clock, no run score. What the trainer already
//    did, kept as the low-pressure default for actually learning a skill.
//  - survival: start with a few lives, a wrong answer costs one, the run
//    ends at zero. The score is how many you got right before that.
//  - blitz:    a fixed clock, answer as many as possible. Wrong answers
//    cost time rather than ending the run, so the pressure is pace, not
//    caution.
//
// This class owns *only* the run's own state (lives, clock, run score) and
// the auto-advance timing. It deliberately doesn't know about Game,
// GameManager or ProgressManager: the editor drives it and reacts to its
// callbacks. That keeps the whole mode/lives/timer state machine testable
// without a message loop, an audio device, or a Component - the same
// reasoning that put ProgressManager::registerAnswer() behind a direct
// synchronous entry point (see docs/testing-strategy.md).
class SessionManager
{
public:
    enum class Mode
    {
        practice,
        survival,
        blitz,

        // A battle with a virtual listener (BotListener, ADR 046): seven
        // rounds, the same round for both, the bot answering from its
        // hearing profile at the round's level. Not remembered per
        // exercise - a battle is started from Live, not left switched on.
        duel
    };

    static constexpr int duelRounds = 7;

    static constexpr int survivalLives = 3;
    static constexpr int blitzSeconds = 90;

    // Wrong answers in Blitz cost time instead of ending the run.
    static constexpr int blitzPenaltySeconds = 5;

    // How long the answer stays on screen before the next round starts.
    // Long enough to read the feedback, short enough not to feel like
    // waiting - the whole point of auto-advance is that a training run
    // shouldn't need a button press between every question.
    static constexpr int autoAdvanceMsCorrect = 900;
    static constexpr int autoAdvanceMsWrong = 1900;   // longer: there's more to take in

    // What a run is made of, when the player has chosen otherwise (the
    // "Pro" settings, see TrainerSettings). The constants above are the
    // defaults and what a beginner always gets: a rule that changes under
    // you is not a rule you can learn to beat.
    struct Rules
    {
        int survivalLives = SessionManager::survivalLives;
        int blitzSeconds = SessionManager::blitzSeconds;
        int blitzPenaltySeconds = SessionManager::blitzPenaltySeconds;
        float answerPauseScale = 1.0f;     // multiplies both auto-advance delays
        bool hintsAllowed = true;
    };

    // Takes effect at the next run, never in the middle of one: a Blitz
    // clock that grew by a minute half-way through is a score that means
    // nothing.
    void setRules (const Rules& newRules) noexcept { pendingRules = newRules; }
    const Rules& getRules() const noexcept { return rules; }

    void setMode (Mode newMode);
    Mode getMode() const noexcept { return mode; }

    // Starts a fresh run in the current mode. Practice runs are always
    // "active"; survival/blitz runs end on their own terms.
    void startRun();
    void endRun();

    bool isRunActive() const noexcept { return runActive; }
    int getLivesRemaining() const noexcept { return livesRemaining; }
    int getRunScore() const noexcept { return runScore; }

    // Points, weighted by precision (the author's call): a right answer is
    // worth 1, and on a ruler exercise up to 0.9 more for how close to the
    // target it landed - "I heard the octave" and "I heard the frequency"
    // are not the same answer. In tenths, so the arithmetic is exact.
    int getRunPointsTenths() const noexcept { return runPointsTenths; }
    float getRunPoints() const noexcept { return (float) runPointsTenths / 10.0f; }
    int getLastPointsTenths() const noexcept { return lastPointsTenths; }

    // The tenths a correct answer earns: 10, plus 0..9 for precision (0..1)
    // when the answer was placed on a scale. `precision` < 0 means a
    // categorical answer, which is right or wrong and earns the flat 10.
    static int pointsTenthsFor (bool wasCorrect, float precision) noexcept
    {
        if (! wasCorrect)
            return 0;

        return 10 + (precision >= 0.0f ? juce::roundToInt (juce::jlimit (0.0f, 1.0f, precision) * 9.0f) : 0);
    }
    int getRoundsThisRun() const noexcept { return roundsThisRun; }

    // The longest run of correct answers *in this run*, which is what a
    // results screen means by "best streak". ProgressManager keeps a
    // lifetime best per exercise; that is a different number and using it
    // here would report a record from last week as if it had just
    // happened.
    int getBestStreakThisRun() const noexcept { return bestStreakThisRun; }

    // Seconds left in a Blitz run; 0 in other modes.
    int getSecondsRemaining() const noexcept { return secondsRemaining; }

    // Call once per answer. Returns true if the run is still going
    // afterwards, false if this answer ended it.
    bool registerAnswer (bool wasCorrect, float precision = -1.0f);

    // ---- duel ----
    void setOpponent (BotListener::Bot bot) noexcept { opponent = bot; }
    BotListener::Bot getOpponent() const noexcept { return opponent; }
    int getOpponentScore() const noexcept { return opponentScore; }
    bool getLastOpponentAnswer() const noexcept { return lastOpponentRight; }

    // One duel round: the player's answer and the bot's. Ends the run after
    // duelRounds. Returns true if the run is still going.
    bool registerDuelRound (bool playerRight, bool botRight, float precision = -1.0f);

    enum class Outcome { won, lost, draw };
    Outcome getDuelOutcome() const noexcept
    {
        return runScore > opponentScore ? Outcome::won : runScore < opponentScore ? Outcome::lost : Outcome::draw;
    }

    // Call once a second while a run is active. Returns true if this tick
    // ended the run (Blitz clock hit zero).
    bool tickOneSecond();

    // How long the editor should wait before starting the next round,
    // given the answer just shown. 0 means "don't auto-advance" (the run
    // is over - the player should see the result, not be thrown into
    // another question).
    int getAutoAdvanceDelayMs (bool wasCorrect) const noexcept;

    // ---- hints ----
    // The scope hint shows the answer's shape, so it has to cost
    // something or it replaces the exercise. What it costs is the mode's
    // own currency: nothing in Practice (the mode for learning, where
    // seeing the shape *is* the lesson), a life in Survival, seconds in
    // Blitz.
    static constexpr int blitzHintSeconds = 10;

    bool isHintFree() const noexcept { return mode == Mode::practice && rules.hintsAllowed; }

    // No hints in a battle: the bot gets none either.
    bool areHintsAllowed() const noexcept { return rules.hintsAllowed && mode != Mode::duel; }

    // Pays for a hint out of the current run. Returns false if the run is
    // over or the cost can't be met, in which case nothing was spent and
    // the hint must not be shown.
    bool spendHint();

    // Fired when a survival/blitz run ends, with the final score, so the
    // editor can record it against the current exercise.
    std::function<void (int finalScore)> onRunEnded;

private:
    Rules rules, pendingRules;
    Mode mode = Mode::practice;
    bool runActive = true;
    int livesRemaining = survivalLives;
    int runScore = 0;
    int runPointsTenths = 0;
    int lastPointsTenths = 0;
    int roundsThisRun = 0;
    int currentStreak = 0;
    int bestStreakThisRun = 0;
    int secondsRemaining = 0;
    BotListener::Bot opponent = BotListener::Bot::hound;
    int opponentScore = 0;
    bool lastOpponentRight = false;
};

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

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
        blitz
    };

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

    void setMode (Mode newMode);
    Mode getMode() const noexcept { return mode; }

    // Starts a fresh run in the current mode. Practice runs are always
    // "active"; survival/blitz runs end on their own terms.
    void startRun();
    void endRun();

    bool isRunActive() const noexcept { return runActive; }
    int getLivesRemaining() const noexcept { return livesRemaining; }
    int getRunScore() const noexcept { return runScore; }
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
    bool registerAnswer (bool wasCorrect);

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

    bool isHintFree() const noexcept { return mode == Mode::practice; }

    // Pays for a hint out of the current run. Returns false if the run is
    // over or the cost can't be met, in which case nothing was spent and
    // the hint must not be shown.
    bool spendHint();

    // Fired when a survival/blitz run ends, with the final score, so the
    // editor can record it against the current exercise.
    std::function<void (int finalScore)> onRunEnded;

private:
    Mode mode = Mode::practice;
    bool runActive = true;
    int livesRemaining = survivalLives;
    int runScore = 0;
    int roundsThisRun = 0;
    int currentStreak = 0;
    int bestStreakThisRun = 0;
    int secondsRemaining = 0;
};

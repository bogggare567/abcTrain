#include "SessionManager.h"

void SessionManager::setMode (Mode newMode)
{
    if (newMode == mode)
        return;

    mode = newMode;
    startRun();
}

void SessionManager::startRun()
{
    runActive = true;
    runScore = 0;
    roundsThisRun = 0;
    livesRemaining = mode == Mode::survival ? survivalLives : 0;
    secondsRemaining = mode == Mode::blitz ? blitzSeconds : 0;
}

void SessionManager::endRun()
{
    if (! runActive)
        return;

    runActive = false;

    // Practice has no score worth recording - it's the mode you use when
    // you're trying to learn something rather than post a number.
    if (mode != Mode::practice && onRunEnded != nullptr)
        onRunEnded (runScore);
}

bool SessionManager::registerAnswer (bool wasCorrect)
{
    if (! runActive)
        return false;

    ++roundsThisRun;

    if (wasCorrect)
        ++runScore;

    switch (mode)
    {
        case Mode::practice:
            break;

        case Mode::survival:
            if (! wasCorrect)
            {
                --livesRemaining;
                if (livesRemaining <= 0)
                {
                    livesRemaining = 0;
                    endRun();
                    return false;
                }
            }
            break;

        case Mode::blitz:
            // Time penalty rather than a lost life: Blitz is about pace,
            // so a wrong answer should cost you the thing you're short of.
            if (! wasCorrect)
            {
                secondsRemaining -= blitzPenaltySeconds;
                if (secondsRemaining <= 0)
                {
                    secondsRemaining = 0;
                    endRun();
                    return false;
                }
            }
            break;
    }

    return true;
}

bool SessionManager::tickOneSecond()
{
    if (! runActive || mode != Mode::blitz)
        return false;

    if (--secondsRemaining <= 0)
    {
        secondsRemaining = 0;
        endRun();
        return true;
    }

    return false;
}

int SessionManager::getAutoAdvanceDelayMs (bool wasCorrect) const noexcept
{
    if (! runActive)
        return 0;

    return wasCorrect ? autoAdvanceMsCorrect : autoAdvanceMsWrong;
}

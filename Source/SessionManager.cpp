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
    rules = pendingRules;

    currentStreak = 0;
    bestStreakThisRun = 0;

    runActive = true;
    runScore = 0;
    runPointsTenths = 0;
    lastPointsTenths = 0;
    roundsThisRun = 0;
    livesRemaining = mode == Mode::survival ? rules.survivalLives : 0;
    secondsRemaining = mode == Mode::blitz ? rules.blitzSeconds : 0;
    opponentScore = 0;
    lastOpponentRight = false;
    playerHp = opponentHp = startHp;
    lastPlayerDamage = lastOpponentDamage = 0.0f;
    playerHpHistory.clear();
    opponentHpHistory.clear();
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

bool SessionManager::registerAnswer (bool wasCorrect, float precision)
{
    if (! runActive)
        return false;

    ++roundsThisRun;
    lastPointsTenths = pointsTenthsFor (wasCorrect, precision);
    runPointsTenths += lastPointsTenths;

    if (wasCorrect)
    {
        ++runScore;
        bestStreakThisRun = juce::jmax (bestStreakThisRun, ++currentStreak);
    }
    else
    {
        currentStreak = 0;
    }

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

        case Mode::duel:
            break;   // registerDuelRound ends it

        case Mode::blitz:
            // Time penalty rather than a lost life: Blitz is about pace,
            // so a wrong answer should cost you the thing you're short of.
            if (! wasCorrect)
            {
                secondsRemaining -= rules.blitzPenaltySeconds;
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

bool SessionManager::registerDuelRound (bool playerRight, bool botRight, float precision)
{
    return registerBattleRound (playerRight ? 0.0f : 2.0f, botRight ? 0.0f : 2.0f, precision);
}

bool SessionManager::registerExternalRound (float playerError, float playerHpNow, float opponentHpNow, bool opponentRight)
{
    if (! runActive || mode != Mode::duel)
        return false;

    lastPlayerDamage = juce::jmax (0.0f, playerHp - playerHpNow);
    lastOpponentDamage = juce::jmax (0.0f, opponentHp - opponentHpNow);
    playerHp = juce::jmax (0.0f, playerHpNow);
    opponentHp = juce::jmax (0.0f, opponentHpNow);
    playerHpHistory.push_back (playerHp);
    opponentHpHistory.push_back (opponentHp);

    lastOpponentRight = opponentRight;
    opponentScore += opponentRight ? 1 : 0;
    registerAnswer (playerError <= 1.0f, -1.0f);

    if (roundsThisRun >= duelRounds || playerHp <= 0.0f || opponentHp <= 0.0f)
    {
        endRun();
        return false;
    }

    return true;
}

bool SessionManager::registerBattleRound (float playerError, float opponentError, float precision)
{
    if (! runActive || mode != Mode::duel)
        return false;

    lastPlayerDamage = damageFor (playerError);
    lastOpponentDamage = damageFor (opponentError);
    playerHp = juce::jmax (0.0f, playerHp - lastPlayerDamage);
    opponentHp = juce::jmax (0.0f, opponentHp - lastOpponentDamage);
    playerHpHistory.push_back (playerHp);
    opponentHpHistory.push_back (opponentHp);

    lastOpponentRight = opponentError <= 1.0f;
    opponentScore += lastOpponentRight ? 1 : 0;
    registerAnswer (playerError <= 1.0f, precision);

    if (roundsThisRun >= duelRounds || playerHp <= 0.0f || opponentHp <= 0.0f)
    {
        endRun();
        return false;
    }

    return true;
}

bool SessionManager::spendHint()
{
    if (! runActive || ! areHintsAllowed())
        return false;

    switch (mode)
    {
        case Mode::practice:
            return true;      // free, by design

        case Mode::survival:
            // Refuse rather than end the run on the hint itself: losing
            // your last life to a hint you asked for, instead of to an
            // answer you got wrong, would feel like the app cheated.
            if (livesRemaining <= 1)
                return false;

            --livesRemaining;
            return true;

        case Mode::blitz:
            if (secondsRemaining <= blitzHintSeconds)
                return false;

            secondsRemaining -= blitzHintSeconds;
            return true;

        case Mode::duel:
            return false;
    }

    return false;
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

    return juce::roundToInt ((float) (wasCorrect ? autoAdvanceMsCorrect : autoAdvanceMsWrong)
                             * rules.answerPauseScale);
}

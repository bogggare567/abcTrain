#include <juce_core/juce_core.h>
#include "../Source/SessionManager.h"

// SessionManager is pure state - no Component, no APVTS, no message loop -
// so unlike the Game -> ProgressManager listener chain (see
// docs/testing-strategy.md) every one of these paths can be driven
// directly and asserted on synchronously.
class SessionManagerTest : public juce::UnitTest
{
public:
    SessionManagerTest() : juce::UnitTest ("SessionManager", "Progress") {}

    void runTest() override
    {
        beginTest ("practice mode never ends and tracks no lives");
        {
            SessionManager session;
            session.setMode (SessionManager::Mode::practice);
            session.startRun();

            for (int i = 0; i < 20; ++i)
                expect (session.registerAnswer (i % 2 == 0));

            expect (session.isRunActive());
            expectEquals (session.getRoundsThisRun(), 20);
            expectEquals (session.getRunScore(), 10);
            expectEquals (session.getLivesRemaining(), 0);
        }

        beginTest ("survival loses one life per wrong answer and ends at zero");
        {
            SessionManager session;
            int reportedScore = -1;
            session.onRunEnded = [&reportedScore] (int score) { reportedScore = score; };

            session.setMode (SessionManager::Mode::survival);
            session.startRun();
            expectEquals (session.getLivesRemaining(), SessionManager::survivalLives);

            expect (session.registerAnswer (true));
            expect (session.registerAnswer (true));
            expectEquals (session.getLivesRemaining(), SessionManager::survivalLives);
            expectEquals (session.getRunScore(), 2);

            expect (session.registerAnswer (false));
            expectEquals (session.getLivesRemaining(), SessionManager::survivalLives - 1);

            expect (session.registerAnswer (false));
            expectEquals (session.getLivesRemaining(), SessionManager::survivalLives - 2);

            // The life that ends the run.
            expect (! session.registerAnswer (false));
            expectEquals (session.getLivesRemaining(), 0);
            expect (! session.isRunActive());

            // Score is what you got right before running out, and it's
            // reported exactly once, through onRunEnded.
            expectEquals (reportedScore, 2);
        }

        beginTest ("answers after a survival run has ended are ignored");
        {
            SessionManager session;
            session.setMode (SessionManager::Mode::survival);
            session.startRun();

            for (int i = 0; i < SessionManager::survivalLives; ++i)
                session.registerAnswer (false);

            expect (! session.isRunActive());

            const auto scoreAtEnd = session.getRunScore();
            const auto roundsAtEnd = session.getRoundsThisRun();

            expect (! session.registerAnswer (true));
            expectEquals (session.getRunScore(), scoreAtEnd);
            expectEquals (session.getRoundsThisRun(), roundsAtEnd);
        }

        beginTest ("blitz spends time rather than lives, and ends when the clock runs out");
        {
            SessionManager session;
            int reportedScore = -1;
            session.onRunEnded = [&reportedScore] (int score) { reportedScore = score; };

            session.setMode (SessionManager::Mode::blitz);
            session.startRun();
            expectEquals (session.getSecondsRemaining(), SessionManager::blitzSeconds);
            expectEquals (session.getLivesRemaining(), 0);

            expect (session.registerAnswer (true));
            expectEquals (session.getSecondsRemaining(), SessionManager::blitzSeconds);

            expect (session.registerAnswer (false));
            expectEquals (session.getSecondsRemaining(),
                          SessionManager::blitzSeconds - SessionManager::blitzPenaltySeconds);

            // Run the clock down to one second left, checking no tick
            // before the last one reports the run as over.
            while (session.getSecondsRemaining() > 1)
                expect (! session.tickOneSecond());

            expect (session.tickOneSecond());
            expectEquals (session.getSecondsRemaining(), 0);
            expect (! session.isRunActive());
            expectEquals (reportedScore, 1);
        }

        beginTest ("ticking does nothing outside blitz");
        {
            SessionManager session;
            session.setMode (SessionManager::Mode::survival);
            session.startRun();

            expect (! session.tickOneSecond());
            expect (session.isRunActive());
            expectEquals (session.getSecondsRemaining(), 0);
        }

        beginTest ("switching mode starts a fresh run");
        {
            SessionManager session;
            session.setMode (SessionManager::Mode::survival);
            session.startRun();
            session.registerAnswer (true);
            session.registerAnswer (false);

            session.setMode (SessionManager::Mode::blitz);

            expect (session.isRunActive());
            expectEquals (session.getRunScore(), 0);
            expectEquals (session.getRoundsThisRun(), 0);
            expectEquals (session.getSecondsRemaining(), SessionManager::blitzSeconds);
        }

        beginTest ("auto-advance waits longer after a wrong answer, and not at all once the run is over");
        {
            SessionManager session;
            session.setMode (SessionManager::Mode::practice);
            session.startRun();

            expectEquals (session.getAutoAdvanceDelayMs (true), SessionManager::autoAdvanceMsCorrect);
            expectEquals (session.getAutoAdvanceDelayMs (false), SessionManager::autoAdvanceMsWrong);
            expect (SessionManager::autoAdvanceMsWrong > SessionManager::autoAdvanceMsCorrect);

            session.endRun();
            expectEquals (session.getAutoAdvanceDelayMs (true), 0);
            expectEquals (session.getAutoAdvanceDelayMs (false), 0);
        }

        beginTest ("a hint is free in practice, and costs nothing that can be spent");
        {
            SessionManager session;
            session.setMode (SessionManager::Mode::practice);
            session.startRun();

            expect (session.isHintFree());

            for (int i = 0; i < 5; ++i)
                expect (session.spendHint());

            expectEquals (session.getLivesRemaining(), 0);
            expectEquals (session.getSecondsRemaining(), 0);
            expect (session.isRunActive());
        }

        beginTest ("a hint costs a life in survival, but never the last one");
        {
            SessionManager session;
            session.setMode (SessionManager::Mode::survival);
            session.startRun();

            expect (! session.isHintFree());

            // Three lives: two hints are affordable, the third is not,
            // because spending down to zero would end the run on a hint
            // the player asked for rather than on a wrong answer.
            expect (session.spendHint());
            expectEquals (session.getLivesRemaining(), SessionManager::survivalLives - 1);

            expect (session.spendHint());
            expectEquals (session.getLivesRemaining(), 1);

            expect (! session.spendHint());
            expectEquals (session.getLivesRemaining(), 1);
            expect (session.isRunActive());
        }

        beginTest ("a hint costs seconds in blitz, and is refused when it would end the run");
        {
            SessionManager session;
            session.setMode (SessionManager::Mode::blitz);
            session.startRun();

            expect (session.spendHint());
            expectEquals (session.getSecondsRemaining(),
                          SessionManager::blitzSeconds - SessionManager::blitzHintSeconds);

            // Wind down to exactly the hint's cost - at which point it
            // must be refused rather than zeroing the clock.
            while (session.getSecondsRemaining() > SessionManager::blitzHintSeconds)
                session.tickOneSecond();

            const auto before = session.getSecondsRemaining();
            expect (! session.spendHint());
            expectEquals (session.getSecondsRemaining(), before);
            expect (session.isRunActive());
        }

        beginTest ("a hint can't be bought once the run is over");
        {
            SessionManager session;
            session.setMode (SessionManager::Mode::survival);
            session.startRun();

            for (int i = 0; i < SessionManager::survivalLives; ++i)
                session.registerAnswer (false);

            expect (! session.isRunActive());
            expect (! session.spendHint());
        }

        beginTest ("a practice run ending reports no score");
        {
            SessionManager session;
            bool reported = false;
            session.onRunEnded = [&reported] (int) { reported = true; };

            session.setMode (SessionManager::Mode::practice);
            session.startRun();
            session.registerAnswer (true);
            session.endRun();

            expect (! reported);
        }

        beginTest ("best streak this run is the run's own, and resets with it");
        {
            SessionManager session;
            session.setMode (SessionManager::Mode::practice);
            session.startRun();

            expectEquals (session.getBestStreakThisRun(), 0);

            session.registerAnswer (true);
            session.registerAnswer (true);
            session.registerAnswer (true);
            expectEquals (session.getBestStreakThisRun(), 3);

            // A wrong answer breaks the streak but not the record of it.
            session.registerAnswer (false);
            expectEquals (session.getBestStreakThisRun(), 3);

            session.registerAnswer (true);
            session.registerAnswer (true);
            expectEquals (session.getBestStreakThisRun(), 3,
                           "a shorter later streak must not replace the best");

            for (int i = 0; i < 4; ++i)
                session.registerAnswer (true);

            expectEquals (session.getBestStreakThisRun(), 6);

            // The next run starts clean: this is *this run's* streak, and
            // reporting last run's would make the results screen lie.
            session.startRun();
            expectEquals (session.getBestStreakThisRun(), 0);
        }
    }
};

static SessionManagerTest sessionManagerTest;

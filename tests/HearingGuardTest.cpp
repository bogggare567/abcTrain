#include <juce_core/juce_core.h>
#include "../Source/HearingGuard.h"
#include "../Source/TrainerSettings.h"
#include "../Source/SessionManager.h"
#include "shared/analysis/AWeightedMeter.h"

// ADR 036: the hearing guard, the A curve it relies on, and the settings
// model that turns it (and every Pro rule) on and off.
class HearingGuardTest : public juce::UnitTest
{
public:
    HearingGuardTest() : juce::UnitTest ("HearingGuard", "Hearing") {}

    static juce::PropertiesFile::Options makeOptions (const juce::String& suffix)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "EarTrainerTests";
        options.filenameSuffix = "settings";
        options.folderName = "EarTrainerTests_Hearing_" + suffix;
        options.commonToAllUsers = false;
        options.getDefaultFile().deleteFile();
        return options;
    }

    // A reading at `levelDb` SPL for a guard calibrated at 80 dB(A) with a
    // reference mean square of 1e-3.
    static double meanSquareFor (double levelDb) { return 1.0e-3 * std::pow (10.0, (levelDb - 80.0) / 10.0); }

    static HearingGuard::Config calibrated()
    {
        HearingGuard::Config c;
        c.calibrationDb = 80;
        return c;
    }

    void runTest() override
    {
        beginTest ("the weekly allowance is ITU-T H.870's 1.6 Pa^2*h at 80 dB(A), 0.51 at 75");
        {
            expectWithinAbsoluteError (HearingGuard::weeklyAllowance (80), 1.6, 0.001);
            expectWithinAbsoluteError (HearingGuard::weeklyAllowance (75), 0.506, 0.001);
        }

        beginTest ("the A curve matches IEC 61672 at 48 kHz");
        {
            // Table values: 100 Hz -19.1, 1 kHz 0, 4 kHz +1.0, 10 kHz -2.5.
            expectWithinAbsoluteError (AWeightedMeter::responseDb (100.0, 48000.0), -19.1f, 0.3f);
            expectWithinAbsoluteError (AWeightedMeter::responseDb (1000.0, 48000.0), 0.0f, 0.01f);
            expectWithinAbsoluteError (AWeightedMeter::responseDb (4000.0, 48000.0), 1.0f, 0.3f);
            expectWithinAbsoluteError (AWeightedMeter::responseDb (10000.0, 48000.0), -2.5f, 0.6f);
            expect (AWeightedMeter::responseDb (31.5, 48000.0) < -35.0f);
        }

        beginTest ("the meter reads a full-scale 1 kHz sine at half the square's energy");
        {
            juce::AudioBuffer<float> sine (2, 48000);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 48000; ++i)
                    sine.setSample (ch, i, (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / 48000.0));

            expectWithinAbsoluteError (AWeightedMeter::meanSquareOf (sine, 48000.0), 0.5, 0.01);
        }

        beginTest ("forty hours at 80 dB(A) is exactly the week");
        {
            juce::PropertiesFile file (makeOptions ("forty"));
            HearingGuard guard (file);
            guard.setConfig (calibrated());
            guard.setReferenceMeanSquare (1.0e-3);

            // Spread over five days, an hour at a time.
            for (int day = 0; day < 5; ++day)
                for (int h = 0; h < 8; ++h)
                    guard.tick (3600.0, meanSquareFor (80.0), 100 + day);

            expectWithinAbsoluteError (guard.weeklyFraction (104), 1.0, 0.001);
            expectWithinAbsoluteError (guard.lastLevelDbA(), 80.0, 0.001);
        }

        beginTest ("+3 dB halves the time: 20 hours at 83 dB(A) is the same week");
        {
            juce::PropertiesFile file (makeOptions ("plus3"));
            HearingGuard guard (file);
            guard.setConfig (calibrated());
            guard.setReferenceMeanSquare (1.0e-3);

            guard.tick (20.0 * 3600.0, meanSquareFor (83.0), 200);
            expectWithinAbsoluteError (guard.weeklyFraction (200), 1.0, 0.03);
        }

        beginTest ("the week is seven days: an old day stops counting");
        {
            juce::PropertiesFile file (makeOptions ("window"));
            HearingGuard guard (file);
            guard.setConfig (calibrated());
            guard.setReferenceMeanSquare (1.0e-3);

            guard.tick (10.0 * 3600.0, meanSquareFor (80.0), 300);
            expectWithinAbsoluteError (guard.weeklyFraction (306), 0.25, 0.001);
            expectEquals (guard.weeklyFraction (307), 0.0);
        }

        beginTest ("half and the full dose are each announced once, when crossed");
        {
            juce::PropertiesFile file (makeOptions ("events"));
            HearingGuard guard (file);
            guard.setConfig (calibrated());
            guard.setReferenceMeanSquare (1.0e-3);

            int halves = 0, fulls = 0;

            for (int h = 0; h < 45; ++h)
                for (auto e : guard.tick (3600.0, meanSquareFor (80.0), 400))
                {
                    halves += e == HearingGuard::Event::doseHalf;
                    fulls += e == HearingGuard::Event::doseFull;
                }

            expectEquals (halves, 1);
            expectEquals (fulls, 1);
        }

        beginTest ("not calibrated: no dose from sound, but typed-in exposure still counts");
        {
            juce::PropertiesFile file (makeOptions ("uncalibrated"));
            HearingGuard guard (file);
            guard.setConfig ({});
            guard.setReferenceMeanSquare (1.0e-3);

            guard.tick (3600.0, meanSquareFor (100.0), 500);
            expectEquals (guard.weeklyFraction (500), 0.0);
            expect (! guard.isCalibrated());

            guard.addExposure (4.0, 80.0, 500);
            expectWithinAbsoluteError (guard.weeklyFraction (500), 0.1, 0.001);
        }

        beginTest ("a break is suggested after the set minutes of sound, and only once");
        {
            juce::PropertiesFile file (makeOptions ("break"));
            HearingGuard guard (file);
            HearingGuard::Config c;
            c.breakMinutes = 45;
            guard.setConfig (c);

            int breaks = 0;
            int firstAt = -1;

            for (int s = 1; s <= 120 * 60; ++s)
                for (auto e : guard.tick (1.0, 1.0e-3, 600))
                    if (e == HearingGuard::Event::breakDue)
                    {
                        ++breaks;
                        if (firstAt < 0) firstAt = s;
                    }

            expectEquals (breaks, 1);
            expectEquals (firstAt, 45 * 60);
        }

        beginTest ("ten quiet minutes count as the break; nine do not");
        {
            juce::PropertiesFile file (makeOptions ("rest"));
            HearingGuard guard (file);
            HearingGuard::Config c;
            c.breakMinutes = 30;
            guard.setConfig (c);

            guard.tick (29.0 * 60.0, 1.0e-3, 700);
            guard.tick (9.0 * 60.0, 0.0, 700);
            expectEquals (guard.continuousSeconds(), 29 * 60);

            guard.tick (60.0, 0.0, 700);
            expectEquals (guard.continuousSeconds(), 0);

            // And the count starts afresh: 29 more minutes are not a break.
            bool due = false;
            for (auto e : guard.tick (29.0 * 60.0, 1.0e-3, 700))
                due |= e == HearingGuard::Event::breakDue;
            expect (! due);
        }

        beginTest ("later moves the reminder by exactly the snooze");
        {
            juce::PropertiesFile file (makeOptions ("snooze"));
            HearingGuard guard (file);
            HearingGuard::Config c;
            c.breakMinutes = 30;
            guard.setConfig (c);

            guard.tick (30.0 * 60.0, 1.0e-3, 800);
            guard.snoozeBreak (15);

            bool due = false;
            for (auto e : guard.tick (14.0 * 60.0, 1.0e-3, 800)) due |= e == HearingGuard::Event::breakDue;
            expect (! due);

            for (auto e : guard.tick (60.0, 1.0e-3, 800)) due |= e == HearingGuard::Event::breakDue;
            expect (due);
        }

        beginTest ("fatigue: two steps below the session's best, late, and not twice in half an hour");
        {
            juce::PropertiesFile file (makeOptions ("fatigue"));
            HearingGuard guard (file);
            guard.setConfig ({});

            expect (guard.noteAnswer (0, 6).empty());
            expect (guard.noteAnswer (0, 4).empty());   // too early in the session

            guard.tick (31.0 * 60.0, 1.0e-3, 900);
            expect (guard.noteAnswer (0, 5).empty());   // one step is the staircase working
            expectEquals ((int) guard.noteAnswer (0, 4).size(), 1);
            expect (guard.noteAnswer (0, 3).empty());   // already said
            expect (guard.noteAnswer (1, 1).empty());   // another exercise has its own best
        }

        beginTest ("switched off, it says nothing and counts nothing");
        {
            juce::PropertiesFile file (makeOptions ("off"));
            HearingGuard guard (file);
            auto c = calibrated();
            c.enabled = false;
            guard.setConfig (c);
            guard.setReferenceMeanSquare (1.0e-3);

            expect (guard.tick (100.0 * 3600.0, meanSquareFor (95.0), 1000).empty());
            expectEquals (guard.weeklyFraction (1000), 0.0);
        }

        beginTest ("Beginner answers with the defaults and keeps the Pro values for later");
        {
            juce::PropertiesFile file (makeOptions ("settings"));
            TrainerSettings settings (file);
            using Id = TrainerSettings::Id;

            expect (settings.getMode() == TrainerSettings::Mode::beginner);

            settings.set (Id::stepRule, 4);
            settings.set (Id::survivalLives, 5);
            settings.set (Id::hearingOn, 0);
            expectEquals (settings.get (Id::stepRule), 3);
            expectEquals (settings.get (Id::survivalLives), 3);
            expectEquals (settings.get (Id::hearingOn), 0);   // about the person: honoured in both

            settings.setMode (TrainerSettings::Mode::pro);
            expectEquals (settings.get (Id::stepRule), 4);
            expectEquals (settings.get (Id::survivalLives), 5);

            settings.setMode (TrainerSettings::Mode::beginner);
            expectEquals (settings.stored (Id::stepRule), 4);
        }

        beginTest ("a value the list does not offer is treated as absent");
        {
            juce::PropertiesFile file (makeOptions ("invalid"));
            TrainerSettings settings (file);
            using Id = TrainerSettings::Id;

            settings.setMode (TrainerSettings::Mode::pro);
            settings.set (Id::stepRule, 7);
            settings.set (Id::calibrationDb, 150);
            expectEquals (settings.get (Id::stepRule), 3);
            expectEquals (settings.get (Id::calibrationDb), 0);

            settings.set (Id::calibrationDb, 82);
            expectEquals (settings.get (Id::calibrationDb), 82);
        }

        beginTest ("reset puts every rule back but keeps the calibration");
        {
            juce::PropertiesFile file (makeOptions ("reset"));
            TrainerSettings settings (file);
            using Id = TrainerSettings::Id;

            settings.setMode (TrainerSettings::Mode::pro);
            settings.set (Id::blitzSeconds, 180);
            settings.set (Id::calibrationDb, 79);
            settings.resetAll();

            expectEquals (settings.get (Id::blitzSeconds), 90);
            expectEquals (settings.get (Id::calibrationDb), 79);
        }

        beginTest ("session rules take effect at the next run, never mid-run");
        {
            SessionManager session;
            session.setMode (SessionManager::Mode::survival);
            session.startRun();

            SessionManager::Rules rules;
            rules.survivalLives = 5;
            rules.answerPauseScale = 2.0f;
            session.setRules (rules);
            expectEquals (session.getLivesRemaining(), 3);

            session.startRun();
            expectEquals (session.getLivesRemaining(), 5);
            expectEquals (session.getAutoAdvanceDelayMs (true), SessionManager::autoAdvanceMsCorrect * 2);
        }

        beginTest ("hints switched off cannot be bought, even in Practice");
        {
            SessionManager session;
            SessionManager::Rules rules;
            rules.hintsAllowed = false;
            session.setRules (rules);
            session.startRun();

            expect (! session.spendHint());
            expect (! session.isHintFree());
        }
    }
};

static HearingGuardTest hearingGuardTest;

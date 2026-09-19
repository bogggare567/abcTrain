#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <map>
#include <vector>

// Looks after the ears of the person training (ADR 036).
//
// Two things, and they need different amounts of knowledge:
//
// 1. **Breaks** - needs nothing but a clock. After an hour of sound
//    without a real pause (default; 30-90 min or off in Pro) it suggests
//    ten quiet minutes, which is WHO's advice for anyone listening for
//    long stretches. A quiet stretch of ten minutes counts as the break
//    and restarts the count - whether it was taken on purpose or not.
//    It also notices *fatigue* the way only a trainer can: if, deep into
//    a session, the staircase has fallen two steps below where it stood
//    earlier in the same exercise, that is the ear getting tired, and the
//    message says exactly that - without a word about doing badly.
//
// 2. **The weekly dose** - needs to know dB SPL, which software cannot
//    see. It becomes available once the player has measured one number:
//    the level of our calibration noise (pink, -20 dBFS RMS) at their
//    listening position, in dB(A), with any meter - WHO itself points to
//    phone apps such as NIOSH SLM. From then on every second of output is
//    converted with that one offset and added up in Pa^2*h, the unit of
//    ITU-T H.870: 80 dB(A) for 40 hours a week is 1.6 Pa^2*h, 75 dB(A)
//    (the stricter mode) 0.51. Equal energy, so +3 dB halves the time.
//
// It is an estimate of the trainer's own share of the week and says so:
// a concert shift or a day of mixing is not in it unless typed in, and
// turning the monitor knob after calibrating makes the number wrong.
// None of it ever blocks training - H.870 itself only asks that the
// listener be told.
//
// Pure: no Component, no timer, no audio. The editor calls tick() once a
// second with what AWeightedMeter measured, and gets back the events to
// show. Days are passed in, so the tests can run a week in a loop.
class HearingGuard
{
public:
    struct Config
    {
        bool enabled = true;
        int breakMinutes = 60;         // 0 = no break reminders
        bool fatigueHint = true;
        int weeklyLimitDb = 80;        // 80 or 75, for 40 hours
        int calibrationDb = 0;         // 0 = not calibrated: no dose
    };

    enum class Event { breakDue, fatigue, doseHalf, doseFull };

    explicit HearingGuard (juce::PropertiesFile& fileToUse);

    void setConfig (const Config&);
    const Config& getConfig() const noexcept { return config; }

    // The calibration noise's own A-weighted mean square, as the meter
    // reads it. Set once by the processor, which is the one that knows
    // exactly what it plays.
    void setReferenceMeanSquare (double referenceMeanSquare) noexcept { reference = referenceMeanSquare; }

    // One step of the clock. `meanSquare` is AWeightedMeter's reading for
    // these `seconds`; `day` is a day number (days since 1970-01-01, local).
    std::vector<Event> tick (double seconds, double meanSquare, int day);

    // After each scored answer, for the fatigue check.
    std::vector<Event> noteAnswer (int gameIndex, int level);

    // The player pressed "later" on a break reminder.
    void snoozeBreak (int minutes = 15);

    // Exposure that happened elsewhere - a gig, a session - typed in by
    // hand, so the week means the week.
    void addExposure (double hours, double levelDbA, int day);

    // ---- what the UI shows ----
    bool isCalibrated() const noexcept { return config.calibrationDb > 0 && reference > 0.0; }

    // This week's dose as a fraction of the limit (1.0 = the limit), over
    // the seven days ending on `day`: the trainer's own sound (only once
    // calibrated) plus whatever was typed in.
    double weeklyFraction (int day) const;

    // The level of the last second of sound, in dB(A) SPL, or 0 if unknown
    // or silent.
    double lastLevelDbA() const noexcept { return lastLevel; }

    int continuousSeconds() const noexcept { return juce::roundToInt (continuous); }
    int sessionSeconds() const noexcept { return juce::roundToInt (session); }

    // Pa^2*h for 40 hours at `levelDb`.
    static double weeklyAllowance (int levelDb);

    // dB(A) SPL -> Pa^2
    static double pressureSquared (double levelDbA);

    // Local calendar day number for a time, for callers.
    static int dayNumber (juce::Time);

    // Below this the output counts as silence (about -70 dBFS).
    static constexpr double silenceMeanSquare = 1.0e-7;

    // How long a quiet stretch has to be to count as a break (WHO: ten
    // minutes in every hour).
    static constexpr double restSeconds = 600.0;

    // How deep into a session before a fall counts as fatigue rather than
    // as the staircase doing its ordinary job.
    static constexpr double fatigueAfterSeconds = 30.0 * 60.0;

private:
    double doseOn (int day) const;
    void storeDose (int day, double pa2h);
    void forgetOldDays (int today);

    juce::PropertiesFile& file;
    Config config;
    double reference = 0.0;

    double continuous = 0.0;     // seconds of sound since the last rest
    double quiet = 0.0;          // seconds of silence in a row
    double session = 0.0;        // seconds of sound since launch
    double nextBreakAt = 0.0;    // `continuous` value at which to remind
    double lastLevel = 0.0;
    double lastFatigueAt = -1.0e9;

    std::map<int, int> sessionBest;   // exercise -> highest step this session
};

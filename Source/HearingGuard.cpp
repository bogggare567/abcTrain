#include "HearingGuard.h"

namespace
{
    constexpr const char* dosePrefix = "hearing.dose.";
    constexpr double referencePressure = 20.0e-6;   // Pa, 0 dB SPL
}

HearingGuard::HearingGuard (juce::PropertiesFile& fileToUse) : file (fileToUse) {}

void HearingGuard::setConfig (const Config& newConfig)
{
    const auto breakChanged = newConfig.breakMinutes != config.breakMinutes;
    config = newConfig;

    if (breakChanged)
        nextBreakAt = 0.0;
}

double HearingGuard::pressureSquared (double levelDbA)
{
    const auto p = referencePressure * std::pow (10.0, levelDbA / 20.0);
    return p * p;
}

double HearingGuard::weeklyAllowance (int levelDb)
{
    return pressureSquared ((double) levelDb) * 40.0;
}

int HearingGuard::dayNumber (juce::Time time)
{
    // Local midnight, so a late session counts towards the day the player
    // thinks it is.
    const auto local = time.toMilliseconds() + (juce::int64) time.getUTCOffsetSeconds() * 1000;
    return (int) (local / (24LL * 3600 * 1000));
}

double HearingGuard::doseOn (int day) const
{
    return file.getDoubleValue (dosePrefix + juce::String (day), 0.0);
}

void HearingGuard::storeDose (int day, double pa2h)
{
    file.setValue (dosePrefix + juce::String (day), pa2h);
}

void HearingGuard::forgetOldDays (int today)
{
    // Only the last seven days are ever read; anything older is noise in
    // the settings file.
    const auto keys = file.getAllProperties().getAllKeys();

    for (const auto& key : keys)
        if (key.startsWith (dosePrefix))
            if (key.fromFirstOccurrenceOf (dosePrefix, false, false).getIntValue() <= today - 7)
                file.removeValue (key);
}

double HearingGuard::weeklyFraction (int day) const
{
    // Counted whether or not the trainer is calibrated: exposure typed in
    // by hand is already in dB SPL and needs no calibration to add up.
    double total = 0.0;

    for (int d = day - 6; d <= day; ++d)
        total += doseOn (d);

    return total / weeklyAllowance (config.weeklyLimitDb);
}

std::vector<HearingGuard::Event> HearingGuard::tick (double seconds, double meanSquare, int day)
{
    std::vector<Event> events;

    if (! config.enabled || seconds <= 0.0)
        return events;

    const auto audible = meanSquare > silenceMeanSquare;

    if (audible)
    {
        quiet = 0.0;
        continuous += seconds;
        session += seconds;
    }
    else
    {
        quiet += seconds;

        if (quiet >= restSeconds)
        {
            continuous = 0.0;
            nextBreakAt = 0.0;
        }
    }

    // Breaks.
    if (config.breakMinutes > 0 && audible)
    {
        if (nextBreakAt <= 0.0)
            nextBreakAt = config.breakMinutes * 60.0;

        if (continuous >= nextBreakAt)
        {
            events.push_back (Event::breakDue);
            // Once per stretch; "later" moves it on explicitly.
            nextBreakAt = 1.0e12;
        }
    }

    // Dose.
    lastLevel = 0.0;

    if (isCalibrated() && audible)
    {
        lastLevel = config.calibrationDb + 10.0 * std::log10 (meanSquare / reference);

        const auto before = weeklyFraction (day);
        storeDose (day, doseOn (day) + pressureSquared (lastLevel) * seconds / 3600.0);
        const auto after = weeklyFraction (day);

        if (before < 0.5 && after >= 0.5)
            events.push_back (Event::doseHalf);

        if (before < 1.0 && after >= 1.0)
            events.push_back (Event::doseFull);

        // Written at most once a minute of sound: a settings file is not a
        // log, and a crash loses a minute at worst.
        if (std::fmod (session, 60.0) < seconds)
        {
            forgetOldDays (day);
            file.saveIfNeeded();
        }
    }

    return events;
}

std::vector<HearingGuard::Event> HearingGuard::noteAnswer (int gameIndex, int level)
{
    std::vector<Event> events;
    auto& best = sessionBest[gameIndex];
    best = juce::jmax (best, level);

    if (! config.enabled || ! config.fatigueHint)
        return events;

    // Two steps below this session's own best, late in the session, and
    // not said in the last half hour.
    if (session >= fatigueAfterSeconds
        && level <= best - 2
        && session - lastFatigueAt >= fatigueAfterSeconds)
    {
        lastFatigueAt = session;
        events.push_back (Event::fatigue);
    }

    return events;
}

void HearingGuard::snoozeBreak (int minutes)
{
    nextBreakAt = continuous + minutes * 60.0;
}

void HearingGuard::addExposure (double hours, double levelDbA, int day)
{
    if (hours <= 0.0)
        return;

    storeDose (day, doseOn (day) + pressureSquared (levelDbA) * hours);
    file.saveIfNeeded();
}

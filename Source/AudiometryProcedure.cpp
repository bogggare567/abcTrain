#include "AudiometryProcedure.h"

AudiometryProcedure::AudiometryProcedure (juce::int64 seed)
    : AudiometryProcedure (seed, Config())
{
}

AudiometryProcedure::AudiometryProcedure (juce::int64 seed, Config configToUse)
    : config (configToUse), random (seed)
{
    now.ear = Ear::left;
    now.step = 0;
    startFrequency();
}

void AudiometryProcedure::startFrequency()
{
    level = config.startDb;
    ascending = false;
    floorHeard = 0;
    presentationsHere = 0;
    ascents.clear();

    now.freqHz = order[(size_t) now.step];
    now.retest = now.step == retestStep;
    now.levelDb = level;
    chooseCatch();
}

void AudiometryProcedure::chooseCatch()
{
    // Never two silences in a row: a second one straight after the first
    // tells the person nothing new and makes the test longer.
    now.silent = config.catchOneIn > 0 && ! lastWasCatch && random.nextInt (config.catchOneIn) == 0;
}

int AudiometryProcedure::stepsDone() const noexcept
{
    if (finished)
        return totalSteps;

    return (now.ear == Ear::right ? stepsPerEar : 0) + now.step;
}

void AudiometryProcedure::answer (bool heard)
{
    if (finished)
        return;

    restarted = false;
    ++presentations;
    auto& ear = ears[(size_t) now.ear];

    if (now.silent)
    {
        ++ear.catchTrials;
        lastWasCatch = true;

        if (heard && ++ear.falseAlarms >= 2)
        {
            if (! ear.remeasured)
            {
                restartEar();
                return;
            }

            ear.unreliable = true;
        }

        // The real presentation the silence stood in for comes next, at
        // the same level.
        now.silent = false;
        return;
    }

    lastWasCatch = false;
    ++presentationsHere;
    const auto key = juce::roundToInt (level);

    if (heard)
    {
        if (ascending)
        {
            auto& a = ascents[key];
            ++a.heard;
            ++a.total;

            if (a.heard >= 2)
            {
                record (level);
                return;
            }
        }

        if (level <= config.minDb + 0.01f)
        {
            // Heard at the quietest level there is: there is nowhere lower
            // to go, so two of those are the threshold.
            if (++floorHeard >= 2)
            {
                record (config.minDb);
                return;
            }
        }

        level = juce::jmax (config.minDb, level - config.downDb);
        ascending = false;
    }
    else
    {
        if (ascending)
            ++ascents[key].total;

        if (level >= config.maxDb - 0.01f)
        {
            record (HearingProfile::notHeard);
            return;
        }

        level = juce::jmin (config.maxDb, level + config.upDb);
        ascending = true;
    }

    if (presentationsHere >= config.maxPresentationsPerFrequency)
    {
        // Answers that never settle. The quietest level heard on an ascent
        // at all is the best estimate there is; the retest will usually
        // say the rest.
        for (const auto& [db, a] : ascents)
            if (a.heard > 0)
            {
                record ((float) db);
                return;
            }

        record (level);
        return;
    }

    now.levelDb = level;
    chooseCatch();
}

void AudiometryProcedure::record (float threshold)
{
    auto& ear = ears[(size_t) now.ear];
    const auto index = (size_t) HearingProfile::indexOf (now.freqHz);

    if (now.step == 0)
        ear.first1k = threshold;

    if (now.step == retestStep)
    {
        ear.retest1k = threshold;

        // BSA: when the two agree, the better one stands. When they do not,
        // the result is marked (HearingProfile::retestAgrees) and the
        // better one still stands - it is the one less likely to be
        // fatigue.
        ear.threshold[index] = juce::jmin (ear.first1k, ear.retest1k);
    }
    else
    {
        ear.threshold[index] = threshold;
    }

    nextStep();
}

void AudiometryProcedure::nextStep()
{
    if (++now.step >= stepsPerEar)
    {
        if (now.ear == Ear::right)
        {
            finished = true;
            return;
        }

        now.ear = Ear::right;
        now.step = 0;
    }

    startFrequency();
}

void AudiometryProcedure::restartEar()
{
    auto& ear = ears[(size_t) now.ear];
    const auto catches = ear.catchTrials;

    ear = HearingProfile::Ear();
    ear.catchTrials = catches;
    ear.remeasured = true;

    restarted = true;
    lastWasCatch = false;
    now.step = 0;
    startFrequency();
}

void AudiometryProcedure::fillProfile (HearingProfile& profile) const
{
    profile.left = ears[0];
    profile.right = ears[1];
}

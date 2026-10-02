#pragma once

#include "shared/audio/HearingProfile.h"
#include <juce_core/juce_core.h>
#include <array>
#include <map>

// The audiometry test's rules (ADR 051), with no sound and no screen: the
// screen asks what to present, plays it, and reports "heard" or "not heard".
//
// The procedure is the British Society of Audiology's Recommended
// Procedure for pure-tone air-conduction audiometry (2018), which is the
// modified Hughson-Westlake method of ISO 8253-1:
//
//   - each ear in turn; per ear 1, 2, 3, 4, 6, 8 kHz, then 1 kHz again
//     (the retest), then 500 and 250 Hz;
//   - every frequency starts at a comfortably audible level;
//   - "heard": the next presentation is 10 dB quieter; "not heard": 5 dB
//     louder ("down 10, up 5");
//   - an *ascending* presentation is one reached by going up after a "not
//     heard"; the threshold is the quietest level heard on at least two
//     ascending presentations (two of two, of three, of four...).
//
// Two checks a self-run test needs and a clinic does not, because nobody
// is watching the person's face:
//
//   - catch trials: about one presentation in six is silence. "Heard" on
//     silence is a false alarm; two on one ear and that ear is measured
//     again from the start; two more and it is marked unreliable;
//   - the 1 kHz retest: if it differs from the first 1 kHz by more than
//     5 dB the result says so (fatigue, a headphone that moved, guessing).
//
// Levels are dBFS at the app's output, between -100 and -10. A frequency
// not heard even at -10 is recorded as not heard rather than chased
// higher - this is headphones on a person, not a calibrated audiometer.
class AudiometryProcedure
{
public:
    enum class Ear { left = 0, right = 1 };

    struct Config
    {
        float startDb = -40.0f;
        float maxDb = HearingProfile::maxLevelDb;
        float minDb = HearingProfile::minLevelDb;
        float downDb = 10.0f;
        float upDb = 5.0f;
        int catchOneIn = 6;                  // 0: no catch trials
        int maxPresentationsPerFrequency = 30;
    };

    struct Presentation
    {
        Ear ear = Ear::left;
        float freqHz = 1000.0f;
        float levelDb = -40.0f;
        bool silent = false;        // a catch trial
        bool retest = false;        // the second 1 kHz
        int step = 0;               // 0..stepsPerEar-1 within the ear
    };

    // The order of one ear's frequencies; index 6 is the 1 kHz retest.
    static constexpr int stepsPerEar = 9;
    static constexpr std::array<float, stepsPerEar> order { 1000.0f, 2000.0f, 3000.0f, 4000.0f, 6000.0f,
                                                            8000.0f, 1000.0f, 500.0f, 250.0f };
    static constexpr int retestStep = 6;
    static constexpr int totalSteps = 2 * stepsPerEar;

    explicit AudiometryProcedure (juce::int64 seed);
    AudiometryProcedure (juce::int64 seed, Config);

    const Presentation& current() const noexcept { return now; }
    bool isFinished() const noexcept { return finished; }

    // The person's answer to current().
    void answer (bool heard);

    // Steps done out of totalSteps, for a progress bar.
    int stepsDone() const noexcept;

    // The ear was just restarted because of false alarms; cleared by the
    // next answer. For the screen to say why it is starting over.
    bool justRestartedEar() const noexcept { return restarted; }

    const HearingProfile::Ear& result (Ear ear) const noexcept { return ears[(size_t) ear]; }

    // The thresholds and checks into a profile (names, id and date are the
    // caller's).
    void fillProfile (HearingProfile&) const;

    int presentationsSoFar() const noexcept { return presentations; }

private:
    struct Ascents { int heard = 0, total = 0; };

    void startFrequency();
    void nextStep();
    void record (float threshold);
    void restartEar();
    void chooseCatch();

    Config config;
    juce::Random random;

    std::array<HearingProfile::Ear, 2> ears;
    Presentation now;
    bool finished = false;
    bool restarted = false;

    // The frequency in progress.
    float level = -40.0f;
    bool ascending = false;
    int floorHeard = 0;
    int presentationsHere = 0;
    std::map<int, Ascents> ascents;   // level (whole dB) -> ascending presentations there
    bool lastWasCatch = false;
    int presentations = 0;
};

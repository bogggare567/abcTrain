#pragma once

#include <juce_core/juce_core.h>
#include "shared/learning/MicroLesson.h"
#include <vector>

// One knob, taught and then checked.
//
// The Learner plugins already had lessons: a sequence of steps that set
// parameters for you while text explains what changed. What they never had
// was a *question*. You watched, and then you were done, and nothing
// anywhere knew whether you could now hear the thing the lesson was about.
//
// A module is that lesson plus two things. A prompt to try it yourself,
// and a check: the plugin sets the knob to a value it does not show you,
// plays it, and asks you to dial the same thing by ear. You are graded on
// how close you got, with a band that narrows as the module's tier rises.
//
// **The unit of the accept band differs per knob, and that is the point.**
// Being 5 ms out on a 3 ms attack and on a 100 ms attack are completely
// different mistakes, so attack is graded as a proportion; being 1 dB out
// is the same mistake at any threshold, so threshold is graded in dB. This
// is the same reasoning, and the same ramp helper, as the trainer's
// continuous exercises - see docs/decisions/020-continuous-answers.md.
//
// Pure data and pure functions: no Component, no APVTS, no message loop,
// so tests/TrainingModuleTest.cpp drives every rule directly.
namespace TrainingModule
{
    // Which synthesized sound a module teaches over. Half the value of a
    // module is picking material the knob is actually audible on: attack
    // does nothing you can hear on a sustained pad, and pre-delay is
    // invisible inside a busy loop. See shared/learning/LessonAudioBed.h.
    enum class Bed
    {
        drumLoop,   // transients, spaced - attack, release, knee, ratio
        bassNote,   // sustained and low - threshold, low shelves
        singleHit,  // one hit then silence - pre-delay, decay tails
        brightHit,  // repeated bright transient - damping, air, high shelves
        chord,      // three detuned voices - width, mid range
        pinkNoise   // flat and dense - frequency, Q
    };

    // How the distance between the hidden target and the player's answer is
    // measured. Each knob gets the one that matches what a mistake means
    // for it.
    enum class Unit
    {
        decibels,       // |a - t| in dB. Threshold, makeup, band gain.
        proportion,     // |ln(a / t)| as a fraction. Attack, release, decay.
        octaves,        // |log2(a / t)|. Frequency.
        rangeFraction,  // |a - t| over the range. Damping, size, mix, width.
        choice          // exact match. Reverb type, shelf vs bell.
    };

    struct Check
    {
        // The knob under test, by its APVTS parameter ID.
        juce::String parameterID;

        Bed bed = Bed::drumLoop;

        // The span the hidden target is drawn from. Deliberately narrower
        // than the parameter's full range: a target at the very end of a
        // knob's travel is findable by feel rather than by ear.
        float minTarget = 0.0f;
        float maxTarget = 1.0f;

        // Draw log-uniformly rather than uniformly. True for anything whose
        // perception is ratio-based - times, frequencies - where a uniform
        // draw would put nearly every target in the top octave.
        bool drawLogarithmically = false;

        // Round the drawn target to this step, so the answer stays a value
        // a person could name. 0 leaves it alone.
        float quantiseTo = 0.0f;

        Unit unit = Unit::rangeFraction;

        // The accept band at the module's first and last tier, in the
        // unit above. Ramped geometrically between them.
        float toleranceAtTierOne = 0.25f;
        float toleranceAtTopTier = 0.08f;

        // Unit::choice only: the options offered, in order.
        std::vector<juce::String> choiceLabels;

        // How the value reads to a person: "ms", "dB", ":1", "Hz", "%".
        juce::String unitSuffix;

        // Multiplies the stored parameter before display. 1 for most; a
        // dry/wet stored 0..1 but shown as a percentage needs 100.
        float displayScale = 1.0f;
    };

    struct Definition
    {
        // Stable persistence key. Never renamed: ModuleProgress stores it,
        // so changing one forgets everybody's progress on that module.
        juce::String id;

        // Plain English, not i18n keys - the same call the existing lesson
        // steps and parameter tooltips already made. Those are English-only
        // today (see the i18n section of CLAUDE.md), and a module whose name
        // were translated while its own explanation was not would read worse
        // than one that is honestly all in one language.
        juce::String name;
        juce::String why;        // one line: what this knob is for
        juce::String tryPrompt;  // the do-it-yourself goal, in words

        // The demonstration, reusing the lesson engine that already exists.
        std::vector<LessonStep> demoSteps;

        Check check;
    };

    // The old three-tier ladder. Kept for the grading functions below and
    // for migrating saved progress; the modules themselves now run on the
    // same ten-step staircase as the trainer (see ModuleProgress, ADR 037).
    static constexpr int numTiers = 3;

    // Ten steps, like every exercise in the trainer.
    static constexpr int maxLevel = 10;

    // The accept band at this tier, in the check's own unit.
    float toleranceForTier (const Check&, int tier) noexcept;

    // The accept band at a step of the staircase, 1..10: the check's tier
    // one tolerance at step 1, its top-tier tolerance at step 10, geometric
    // between - the same ramp the trainer's rulers use.
    float toleranceForLevel (const Check&, int level) noexcept;
    bool passesAtLevel (const Check&, float target, float answer, int level) noexcept;
    float qualityAtLevel (const Check&, float target, float answer, int level) noexcept;

    // The values that would pass around `value` at `level`, in the knob's
    // own units - what the check's scale draws as the lit band. Clamped to
    // nothing; the caller clamps to its scale.
    juce::Range<float> acceptRange (const Check&, float value, int level) noexcept;

    // The tolerance and an error, both in what the knob displays: "3 dB",
    // "35%", "0.3 oct". `signedError` is answer minus target for linear
    // units and the ratio as a signed percentage for proportional ones.
    struct Readout { float tolerance = 0.0f; float signedError = 0.0f; bool percent = false; bool octaves = false; };
    Readout readout (const Check&, float target, float answer, int level) noexcept;

    // How far the answer is from the target, in the check's own unit.
    // Always >= 0. A target of zero under a ratio-based unit is a
    // definition error, not a runtime one - such a check should use
    // decibels or rangeFraction instead - so it returns the full range
    // rather than infinity.
    float errorFor (const Check&, float target, float answer) noexcept;

    bool passes (const Check&, float target, float answer, int tier) noexcept;

    // 1 dead on, 0 at the edge of the band and beyond. This is what turns a
    // pass into "how well", the same precision bonus the trainer already
    // pays out for landing close rather than merely landing inside.
    float quality (const Check&, float target, float answer, int tier) noexcept;

    // A hidden target inside the check's span, drawn the way the check
    // says and quantised if it asks. Deliberately takes the Random by
    // reference so a test can seed it and get the same round twice.
    float drawTarget (const Check&, juce::Random&) noexcept;

    // A walkthrough: a whole workflow shown step by step, with no check.
    // It runs in the same panel as the modules (ADR 037); an empty
    // parameterID is what marks it.
    inline Definition walkthrough (juce::String id, juce::String why, Bed bed, const MicroLesson& lesson)
    {
        Definition d;
        d.id = std::move (id);
        d.name = lesson.getTitle();
        d.why = std::move (why);
        d.demoSteps = lesson.getSteps();
        d.check.bed = bed;
        return d;
    }
}

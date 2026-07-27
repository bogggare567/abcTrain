#pragma once

#include <juce_core/juce_core.h>
#include "MicroLesson.h"
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
    // invisible inside a busy loop. See shared/LessonAudioBed.h.
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

        // Unit::choice only: i18n keys for the options offered.
        std::vector<juce::String> choiceKeys;
    };

    struct Definition
    {
        // Stable persistence key. Never renamed: ModuleProgress stores it,
        // so changing one forgets everybody's progress on that module.
        juce::String id;

        juce::String nameKey;
        juce::String whyKey;        // one line: what this knob is for
        juce::String tryPromptKey;  // the do-it-yourself goal, in words

        // The demonstration, reusing the lesson engine that already exists.
        std::vector<LessonStep> demoSteps;

        Check check;
    };

    // Three tiers, not ten. A module is one knob: "roughly", "confidently"
    // and "precisely" is the whole ladder there is, and ten rungs on it
    // would be nine kinds of the same answer.
    static constexpr int numTiers = 3;

    // The accept band at this tier, in the check's own unit.
    float toleranceForTier (const Check&, int tier) noexcept;

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
}

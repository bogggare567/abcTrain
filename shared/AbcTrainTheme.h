#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Design tokens for all four plugins: one place that decides every colour,
// spacing step, corner radius, elevation and animation duration in the UI,
// so a value is never hand-picked twice at a call site.
//
// Why a separate header from AbcTrainLookAndFeel: the LookAndFeel can only
// reach widgets JUCE routes through it (buttons, sliders, combo boxes).
// The custom components - spectrum, waveform, GR meter, choice slider,
// lesson overlay - draw themselves, and previously each hardcoded its own
// copies of #1e1e2e/#5b9bd5/#14141a. Those literals had already drifted
// apart by a shade or two in a few places. Everything now reads
// AbcTrainTheme::current() instead.
//
// The light palette is deliberately NOT an inversion of the dark one:
// inverting a dark UI gives you glaring white panels and accent colours
// that lose all their contrast against them. Light mode uses a warm
// off-white paper tone, deeper and slightly desaturated accents (light
// backgrounds need more colour weight to read at the same visual strength),
// and softer, wider shadows, since shadows on a light ground are ambient
// occlusion rather than glow.
namespace AbcTrainTheme
{
    enum class Mode
    {
        dark,
        light
    };

    struct Palette
    {
        Mode mode;

        // Surfaces, from furthest back to closest to the viewer.
        juce::Colour windowBackground;   // the editor's own backdrop
        juce::Colour panelBackground;    // grouped section backing
        juce::Colour widgetBackground;   // combo boxes, knob caps, buttons
        juce::Colour displayBackground;  // spectrum/waveform wells (darkest)

        juce::Colour outline;            // 1px borders
        juce::Colour divider;            // hairline separators inside groups

        juce::Colour text;               // primary body text
        juce::Colour textDim;            // secondary/caption text
        juce::Colour textBright;         // headings, active values

        // Accents. `accent` is the calm structural colour (spectrum curve,
        // value arcs); `accentWarm` is the "you are touching this" colour.
        juce::Colour accent;
        juce::Colour accentWarm;
        juce::Colour positive;           // correct answer / healthy level
        juce::Colour negative;           // wrong answer / gain reduction

        // Shadow tint and strength differ per mode: on a dark ground a
        // shadow is near-black and tight; on a light ground it's a soft,
        // wide, slightly cool grey, otherwise it reads as a dirty smudge.
        juce::Colour shadow;
        float shadowStrength;

        // Multiplier applied to the noise-texture overlay. Light surfaces
        // show grain far more readily than dark ones, so the same alpha
        // that reads as "subtle tooth" on #1e1e2e looks like dirt on paper.
        float textureStrength;
    };

    const Palette& dark() noexcept;
    const Palette& light() noexcept;

    // The palette every component draws from. Set once per editor (from the
    // persisted preference) before anything paints; components re-read it
    // on each paint() so a mode switch just needs a repaint, not a rebuild.
    //
    // Deliberately a process-wide value rather than per-editor state: a host
    // with LearnerEQ and LearnerComp open at once should not show one in
    // light mode and the other in dark. Only ever touched from the message
    // thread (editor construction and the theme toggle), same threading
    // assumption as LookAndFeel itself.
    const Palette& current() noexcept;
    void setMode (Mode) noexcept;
    Mode getMode() noexcept;

    // The four skill families the whole product is organised around, and
    // the colour each one owns.
    //
    // EarTrainer groups its nine exercises by these on the home screen and
    // tints the training backdrop with them; the three Learner plugins are
    // each squarely one family (EQ is frequency, Comp is dynamics, Verb is
    // space), so each takes its family's colour as its own accent. That is
    // what makes "which plugin am I looking at" answerable before you have
    // read a single word - and it makes the trainer and the plugin that
    // teaches the same skill visibly the same subject.
    //
    // These used to be four hex literals sitting in a helper inside
    // EarTrainer's PluginEditor.cpp, which is exactly the drift this file
    // exists to prevent.
    enum class Family
    {
        frequency,   // EQ, named ranges - LearnerEQ
        dynamics,    // compression, gain - LearnerComp
        space,       // reverb, pan, delay, width - LearnerVerb
        character    // distortion and anything unclassified
    };

    // Adjusted per mode: a light page needs a deeper, slightly less
    // saturated hue to carry the same visual weight as a dark one.
    juce::Colour accentFor (Family) noexcept;

    // Spacing scale - a 4px base grid. Layout code uses these names instead
    // of bare numbers so "the gap between a control and its label" and "the
    // gap between two groups" can't accidentally end up the same value.
    namespace Spacing
    {
        constexpr int hairline = 2;
        constexpr int tight    = 4;
        constexpr int small    = 8;
        constexpr int medium   = 12;
        constexpr int large    = 20;
        constexpr int section  = 28;  // between grouped sections
    }

    namespace Radius
    {
        constexpr float small  = 4.0f;
        constexpr float button = 7.0f;
        constexpr float panel  = 10.0f;
        constexpr float well   = 8.0f;   // spectrum/waveform displays
    }

    // Animation timings, in milliseconds. Slower than a typical web UI on
    // purpose: these read as weighted rather than snappy, which is the
    // "physical" feel the whole pass is after.
    namespace Duration
    {
        constexpr double hover      = 140.0;   // pointer enters/leaves
        constexpr double press      = 90.0;    // press is faster than release
        constexpr double release    = 260.0;   // ...so it settles, not snaps
        constexpr double transition = 320.0;   // panel/game changes
        constexpr double feedback   = 900.0;   // correct-answer glow decay
        constexpr double sway       = 720.0;   // wrong-answer settle
        constexpr double breath     = 3400.0;  // progress-bar idle pulse
    }

    // Easing curves. JUCE's own juce::Animator easings cover most cases;
    // these exist for the hand-rolled Timer-driven interpolations where an
    // Animator per widget would be far too much machinery (see
    // AbcTrainLookAndFeel's widget-state registry).
    // Midpoint between two points. juce::Point has no such method, and
    // both the spectrum curve and the waveform envelope need it for their
    // midpoint-quadratic smoothing.
    inline juce::Point<float> midpoint (juce::Point<float> a, juce::Point<float> b) noexcept
    {
        return { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f };
    }

    namespace Ease
    {
        // Decelerating - things arriving on screen or under the cursor.
        float out (float t) noexcept;
        // Accelerate then decelerate - A-to-B transitions.
        float inOut (float t) noexcept;
        // Overshoots slightly past 1 then settles: mass and springiness.
        float backOut (float t) noexcept;
    }
}

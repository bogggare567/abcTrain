#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <functional>
#include <vector>

/**
    The rules of a round on a third-party plugin (ADR 065, «Свой плагин»).

    No GUI, no plugin, no audio: a knob is described by what the host can
    read about it, and its text comes through a callback, so the rules are
    tested without any plugin installed (tests/OwnPluginRoundTest).

    A round, as in discussion #44: one knob of the player's own setting is
    moved to a hidden position; the player hears A (his setting) and B (the
    hidden one) and names B's value from 2-5 options written in the
    plugin's own words ("30 ms", "4:1", "IRON").

    The options are spread evenly over the knob's travel, not over its
    numbers: a plugin's knob is usually already shaped to the ear (log
    frequency, skewed time), so equal travel is the first approximation of
    equal audible steps. The perceptual scale of stage 2 replaces it.
*/
namespace OwnPluginRound
{
    struct Knob
    {
        int index = -1;
        juce::String name, label;
        int steps = 0x7fffffff;   // AudioProcessorParameter::getNumSteps()
        bool discrete = false, boolean = false, meta = false, automatable = true;
    };

    // Names that are never a sound: power, bypass, listen/solo switches,
    // meters, presets, sidechain routing, oversampling, the UI. Lower case,
    // matched as substrings - bx_glue alone has "Power", "Bypass",
    // "X-Over Listen", "Sidechain Listen", "External SC In", "Preset Bank".
    inline bool nameIsNotASound (const juce::String& name)
    {
        const auto n = name.toLowerCase();
        for (auto* word : { "bypass", "power", "listen", "solo", "mute", "meter", "preset", "bank",
                            "sidechain", "side chain", "sc in", "ext", "oversampl", "quality", "latency",
                            "ui ", "gui", "zoom", "scale", "theme", "midi", "link", "auto output", "auto gain",
                            "auto release", "on/off", " on" , "enable", "active", "analy", "display", "view" })
            if (n.contains (word) || n == juce::String (word).trim())
                return true;
        return false;
    }

    /** A knob a round can be about: it moves the sound, and it has room
        for at least two distinguishable options. */
    inline bool isTrainable (const Knob& k)
    {
        if (k.index < 0 || k.boolean || k.meta || ! k.automatable)
            return false;
        // A knob with no name cannot be asked about (bx_clipper has one).
        if (k.name.trim().isEmpty())
            return false;
        if (k.discrete && k.steps < 2)
            return false;
        return ! nameIsNotASound (k.name);
    }

    /** Knobs whose whole job is level: loudness matching is switched off
        for them, or the answer could never be heard (ADR 055's rule has
        exactly this exception). */
    inline bool isLevelKnob (const Knob& k)
    {
        const auto n = k.name.toLowerCase();

        // A band's gain is tone, not level: "Band 1 Gain", "Low Shelf Gain",
        // "Emphasis Gain" are matched, or the round would be a level tell.
        for (auto* tone : { "band", "shelf", "bell", "low", "high", "mid", "emphasis", "empahsis" /* sic, bx_glue */, "eq", "tilt", "sc ", "sidechain" })
            if (n.contains (tone))
                return false;

        return n.contains ("gain") || n.contains ("output") || n.contains ("volume")
            || n.contains ("level") || n.contains ("makeup") || n.contains ("make-up")
            || n.contains ("headroom") || n.contains ("trim");
    }

    /** Below this, B is the same sound as A on this loop with this setting:
        the knob does nothing here (a band that is switched off, an attack
        on a compressor that never reaches its threshold). Such a round is
        dropped and another knob taken - a round nobody can hear is not a
        hard round, it is a broken one. */
    constexpr float inaudibleBelowDb = -40.0f;

    /** Above this, B is louder or quieter than A by more than the match can
        take back (it stops at 12 dB): the level would answer the round.
        bx's "Ambience" switch does this (OwnPluginSmoke, 2026-10-24). Such
        a knob is dropped like a silent one - unless it is a level knob,
        where level is the answer. */
    constexpr float unmatchableAboveDb = 10.0f;

    /** Normalised positions for the options: evenly over the travel, or
        over the steps for a stepped knob (never between two steps). */
    inline std::vector<float> optionPositions (const Knob& k, int wanted)
    {
        std::vector<float> out;
        const auto stepped = k.discrete && k.steps >= 2 && k.steps < 1000;
        const auto n = stepped ? juce::jmin (wanted, k.steps) : juce::jmax (2, wanted);

        for (int i = 0; i < n; ++i)
        {
            auto v = n == 1 ? 0.0f : (float) i / (float) (n - 1);
            if (stepped)
            {
                // Spread over the steps and land exactly on one.
                const auto step = juce::roundToInt (v * (float) (k.steps - 1));
                v = (float) step / (float) (k.steps - 1);
            }
            if (out.empty() || std::abs (out.back() - v) > 1.0e-4f)
                out.push_back (v);
        }
        return out;
    }

    struct Round
    {
        int knob = -1;                  // index into the plugin's parameters
        juce::String knobName;
        std::vector<float> options;     // normalised
        juce::StringArray optionTexts;  // in the plugin's words
        int correct = -1;               // index into options
        float original = 0.0f;          // the player's own value, restored after
    };

    /** Picks the knob (from the trainable ones the player ticked) and the
        hidden option. Never the option equal to the player's own value:
        then A and B would be the same sound and the round a coin toss.
        Options whose texts are identical are merged (a plugin that writes
        "OFF" for the bottom tenth of a knob). */
    inline Round make (const std::vector<Knob>& knobs, const std::vector<float>& currentValues,
                       int optionCount, juce::Random& random,
                       const std::function<juce::String (int knob, float value)>& textFor,
                       const std::vector<int>& skip = {})
    {
        Round r;
        std::vector<const Knob*> pool;
        for (const auto& k : knobs)
            if (isTrainable (k) && std::find (skip.begin(), skip.end(), k.index) == skip.end())
                pool.push_back (&k);

        if (pool.empty())
            return r;

        // Knobs in random order; the first that yields two options or more.
        for (int i = (int) pool.size() - 1; i > 0; --i)
            std::swap (pool[(size_t) i], pool[(size_t) random.nextInt (i + 1)]);

        for (const auto* candidate : pool)
        {
            const auto& k = *candidate;
            r = {};
            r.knob = k.index;
            r.knobName = k.name;
            r.original = k.index < (int) currentValues.size() ? currentValues[(size_t) k.index] : 0.0f;

            const auto positions = optionPositions (k, optionCount);
            juce::StringArray texts;
            for (auto v : positions)
                texts.add (textFor (k.index, v));

            // Every position reads the same: the plugin does not write a
            // value it is not set to (ValhallaRoom's VST3 answers with its
            // current setting whatever it is asked). Then the travel itself
            // is the text, in percent of the knob's way.
            auto allSame = true;
            for (const auto& t : texts)
                allSame = allSame && t == texts[0];

            for (size_t i = 0; i < positions.size(); ++i)
            {
                const auto text = allSame ? juce::String (juce::roundToInt (positions[i] * 100.0f)) + "%" : texts[(int) i];
                if (! r.optionTexts.contains (text))
                {
                    r.options.push_back (positions[i]);
                    r.optionTexts.add (text);
                }
            }

            if (r.options.size() >= 2)
                break;
        }

        if (r.options.size() < 2)
        {
            r = {};
            return r;
        }

        // Candidates: every option that is not where the player's knob is.
        std::vector<int> candidates;
        for (int i = 0; i < (int) r.options.size(); ++i)
            if (std::abs (r.options[(size_t) i] - r.original) > 0.5f / (float) r.options.size())
                candidates.push_back (i);

        if (candidates.empty())
            for (int i = 0; i < (int) r.options.size(); ++i)
                candidates.push_back (i);

        r.correct = candidates[(size_t) random.nextInt ((int) candidates.size())];
        return r;
    }

    /** How far off an answer was, in options: 0 right, 1 the neighbour. */
    inline int stepsOff (const Round& r, int answer)
    {
        return std::abs (answer - r.correct);
    }
}

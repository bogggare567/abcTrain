#include "Source/OwnPlugins/KnobGlossary.h"
#include <juce_core/juce_core.h>
#include "../Source/OwnPlugins/OwnPluginRound.h"

// The rules of a round on the player's own plugin (ADR 065), on knobs as
// PluginProbe read them from bx_glue on the author's Mac (2026-10-23).
class OwnPluginRoundTest : public juce::UnitTest
{
public:
    OwnPluginRoundTest() : juce::UnitTest ("OwnPluginRound", "OwnPlugins") {}

    static OwnPluginRound::Knob knob (int index, const char* name, int steps = 0x7fffffff, bool boolean = false)
    {
        OwnPluginRound::Knob k;
        k.index = index;
        k.name = name;
        k.steps = steps;
        k.discrete = steps < 1000;
        k.boolean = boolean;
        return k;
    }

    void runTest() override
    {
        // bx_glue's list, trimmed: what a host sees.
        const std::vector<OwnPluginRound::Knob> glue {
            knob (0, "Power", 2, true), knob (1, "Preset Bank", 4), knob (2, "Input Gain"), knob (4, "XL Saturation"),
            knob (6, "Threshold"), knob (7, "Ratio", 3), knob (8, "Attack", 6), knob (15, "External SC In", 2, true),
            knob (24, "X-Over Listen", 2, true), knob (26, "Band Tilt"), knob (29, "Sidechain Listen", 2, true),
            knob (30, "Bypass", 2, true) };

        beginTest ("switches, presets, listen and sidechain are never a round");
        {
            juce::StringArray trainable;
            for (const auto& k : glue)
                if (OwnPluginRound::isTrainable (k))
                    trainable.add (k.name);

            expectEquals (trainable.joinIntoString (","), juce::String ("Input Gain,XL Saturation,Threshold,Ratio,Attack,Band Tilt"));
        }

        beginTest ("level knobs switch loudness matching off, the rest keep it");
        {
            expect (OwnPluginRound::isLevelKnob (knob (2, "Input Gain")));
            expect (OwnPluginRound::isLevelKnob (knob (17, "Headroom")));
            expect (! OwnPluginRound::isLevelKnob (knob (6, "Threshold")));
            expect (! OwnPluginRound::isLevelKnob (knob (8, "Attack")));
            // a band's gain is tone: matched like any other knob
            expect (! OwnPluginRound::isLevelKnob (knob (3, "Band 1 Gain")));
            expect (! OwnPluginRound::isLevelKnob (knob (19, "Empahsis Gain")));
        }

        beginTest ("a stepped knob's options land on its steps, never between");
        {
            const auto ratio = OwnPluginRound::optionPositions (knob (7, "Ratio", 3), 5);
            expectEquals ((int) ratio.size(), 3);
            expectWithinAbsoluteError (ratio[1], 0.5f, 1.0e-5f);

            const auto attack = OwnPluginRound::optionPositions (knob (8, "Attack", 6), 3);
            expectEquals ((int) attack.size(), 3);
            for (auto v : attack)
                expectWithinAbsoluteError (v * 5.0f, std::round (v * 5.0f), 1.0e-4f);
        }

        beginTest ("the hidden option is never the player's own position");
        {
            juce::Random random (7);
            std::vector<float> values (32, 0.5f);
            const auto text = [] (int, float v) { return juce::String (juce::roundToInt (v * 100.0f)); };

            for (int i = 0; i < 200; ++i)
            {
                const auto r = OwnPluginRound::make (glue, values, 5, random, text);
                expect (r.knob >= 0);
                expect (r.correct >= 0 && r.correct < (int) r.options.size());
                expect (std::abs (r.options[(size_t) r.correct] - values[(size_t) r.knob]) > 0.05f);
                expectEquals (r.optionTexts.size(), (int) r.options.size());
            }
        }

        beginTest ("options that read the same are merged; a knob that always reads the same gets percent");
        {
            juce::Random random (3);
            std::vector<float> values (32, 0.0f);
            // A plugin that answers every position with the same text (its
            // current one) gets the travel in percent instead.
            const auto allOff = [] (int, float) { return juce::String ("OFF"); };
            const auto r = OwnPluginRound::make ({ knob (14, "Mono Maker Frequency") }, values, 5, random, allOff);
            expectEquals (r.optionTexts.joinIntoString (" "), juce::String ("0% 25% 50% 75% 100%"));

            const auto offThenNumbers = [] (int, float v) { return v < 0.3f ? juce::String ("OFF") : juce::String (juce::roundToInt (v * 666.0f)); };
            const auto r2 = OwnPluginRound::make ({ knob (14, "Mono Maker Frequency") }, values, 5, random, offThenNumbers);
            expectEquals ((int) r2.options.size(), 4);
        }

        beginTest ("knobs found inaudible are skipped");
        {
            juce::Random random (9);
            std::vector<float> values (32, 0.5f);
            const std::vector<int> skip { 2, 4, 6, 7, 8 };   // all but Band Tilt
            for (int i = 0; i < 20; ++i)
                expectEquals (OwnPluginRound::make (glue, values, 3, random,
                                                    [] (int, float v) { return juce::String (v); }, skip).knobName,
                              juce::String ("Band Tilt"));
        }

        beginTest ("no trainable knob, no round");
        {
            juce::Random random (1);
            std::vector<float> values (32, 0.0f);
            const auto r = OwnPluginRound::make ({ knob (0, "Power", 2, true), knob (30, "Bypass", 2, true) }, values, 3, random,
                                                 [] (int, float) { return juce::String ("x"); });
            expectEquals (r.knob, -1);
        }

        beginTest ("the glossary knows the names plugins share, and not their neighbours");
        {
            const std::pair<const char*, const char*> cases[] {
                { "Threshold", "threshold" }, { "Ratio", "ratio" }, { "Attack", "attack" }, { "Release", "release" },
                { "Pre-Delay", "predelay" }, { "PreDelay", "predelay" }, { "Decay", "decay" }, { "Dry/Wet", "mix" },
                { "Mix", "mix" }, { "Input Gain", "inputgain" }, { "Output", "makeup" }, { "Band 1 Frequency", "frequency" },
                { "Band 1 Gain", "eqgain" }, { "Band 1 Q", "q" }, { "Low Cut", "hpf" }, { "XL Saturation", "drive" },
                { "Band Tilt", "tone" }, { "Delay Time", "delaytime" }, { "Feedback", "feedback" }, { "Knee", "knee" },
                { "Room Size", "size" }, { "HF Damping", "damping" },
                { "EQ Mode", "" }, { "Bypass", "" }, { "Oversampling", "" },
            };
            for (const auto& [name, key] : cases)
                expectEquals (KnobGlossary::keyFor (name), juce::String (key), name);

            // Every key the rules can give has a place in the order.
            for (const auto& k : KnobGlossary::keys())
                expect (KnobGlossary::orderOf (k) < 100, k);
        }
    }
};

static OwnPluginRoundTest ownPluginRoundTest;

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/OwnPlugins/OwnLesson.h"
#include "../Source/LessonLibrary.h"
#include "shared/learning/KnobGlossary.h"
#include <cmath>

// A course lesson on somebody else's plugin (ADR 068): the knob is found by
// its glossary name, the value by what the plugin shows. These knobs show
// their values the ways real plugins do - a U+2212 minus, "1.20 kHz",
// "0.02 s" for 20 ms, "4.0:1" - and the lesson still lands on the number.
class OwnLessonTest : public juce::UnitTest
{
public:
    OwnLessonTest() : juce::UnitTest ("OwnLesson", "OwnPlugins") {}

    // A knob from lo to hi (linear or log), shown by `show`.
    struct Knob : public juce::AudioProcessorParameter
    {
        Knob (juce::String n, double l, double h, bool lg, std::function<juce::String (double)> s, juce::String lab = {})
            : name (std::move (n)), lo (l), hi (h), logTaper (lg), show (std::move (s)), unitLabel (std::move (lab)) {}

        double valueAt (float pos) const
        {
            return logTaper ? lo * std::pow (hi / lo, (double) pos) : lo + (hi - lo) * pos;
        }

        float getValue() const override { return value; }
        void setValue (float v) override { value = v; }
        float getDefaultValue() const override { return 0.5f; }
        juce::String getName (int) const override { return name; }
        juce::String getLabel() const override { return unitLabel; }
        float getValueForText (const juce::String&) const override { return 0.0f; }   // not used, like many plugins'
        juce::String getText (float pos, int) const override { return show (valueAt (pos)); }

        juce::String name;
        double lo, hi;
        bool logTaper;
        std::function<juce::String (double)> show;
        juce::String unitLabel;
        float value = 0.5f;
    };

    void expectWithinRelativeError (double actual, double expected, double relative)
    {
        expect (std::abs (actual - expected) <= std::abs (expected) * relative,
                juce::String (actual) + " is not within " + juce::String (relative * 100.0) + "% of " + juce::String (expected));
    }

    static juce::String minus (double v, int decimals)
    {
        const auto t = juce::String (std::abs (v), decimals);
        return v < 0.0 ? juce::String::fromUTF8 ("\xe2\x88\x92") + t : t;
    }

    void runTest() override
    {
        beginTest ("what a plugin shows, in the unit lessons write");
        {
            using OwnLesson::shownValue;
            expectWithinAbsoluteError (*shownValue (juce::String::fromUTF8 ("\xe2\x88\x92" "20.0 dB"), {}, "threshold"), -20.0, 1.0e-6);
            expectWithinAbsoluteError (*shownValue ("1.20 kHz", {}, "frequency"), 1200.0, 1.0e-6);
            expectWithinAbsoluteError (*shownValue ("0.02 s", {}, "attack"), 20.0, 1.0e-6);
            expectWithinAbsoluteError (*shownValue ("850", "ms", "release"), 850.0, 1.0e-6);
            expectWithinAbsoluteError (*shownValue ("1500 ms", {}, "decay"), 1.5, 1.0e-6);
            expectWithinAbsoluteError (*shownValue ("4.0:1", {}, "ratio"), 4.0, 1.0e-6);
            expectWithinAbsoluteError (*shownValue ("30,5 %", {}, "mix"), 30.5, 1.0e-6);
            expect (*shownValue ("Inf:1", {}, "ratio") > 100.0);
            expect (! shownValue ("Off", {}, "hpf").has_value());
        }

        beginTest ("the knob goes where the plugin shows the number, whatever its taper");
        {
            Knob threshold ("Threshold", -60.0, 0.0, false, [] (double v) { return minus (v, 1) + " dB"; });
            const auto t = OwnLesson::placeAt (threshold, "threshold", -20.0);
            expect (t.has_value() && t->exact);
            expectWithinAbsoluteError (threshold.valueAt (t->position), -20.0, 0.2);

            Knob attack ("Attack", 0.1, 100.0, true, [] (double ms)
                         { return ms >= 1000.0 ? juce::String (ms / 1000.0, 2) + " s" : juce::String (ms, 1) + " ms"; });
            const auto a = OwnLesson::placeAt (attack, "attack", 30.0);
            expect (a.has_value() && a->exact);
            expectWithinRelativeError (attack.valueAt (a->position), 30.0, 0.05);

            Knob release ("Release", 10.0, 5000.0, true, [] (double ms)
                          { return ms >= 1000.0 ? juce::String (ms / 1000.0, 2) + " s" : juce::String (ms, 0) + " ms"; });
            const auto r = OwnLesson::placeAt (release, "release", 1200.0);
            expect (r.has_value() && r->exact);
            expectWithinRelativeError (release.valueAt (r->position), 1200.0, 0.05);

            Knob freq ("Band 3 Frequency", 10.0, 30000.0, true, [] (double hz)
                       { return hz >= 1000.0 ? juce::String (hz / 1000.0, 2) + " kHz" : juce::String (hz, 1) + " Hz"; });
            const auto f = OwnLesson::placeAt (freq, "frequency", 300.0);
            expect (f.has_value() && f->exact);
            expectWithinRelativeError (freq.valueAt (f->position), 300.0, 0.05);

            Knob ratio ("Ratio", 1.0, 20.0, true, [] (double v) { return juce::String (v, 1) + ":1"; });
            const auto q = OwnLesson::placeAt (ratio, "ratio", 4.0);
            expect (q.has_value() && q->exact);
            expectWithinRelativeError (ratio.valueAt (q->position), 4.0, 0.05);
        }

        beginTest ("a value out of the plugin's reach: the nearest, and the step says so");
        {
            Knob shallow ("Thresh", -40.0, 0.0, false, [] (double v) { return juce::String (v, 1) + " dB"; });
            const auto p = OwnLesson::placeAt (shallow, "threshold", -60.0);
            expect (p.has_value() && ! p->exact);
            expectWithinAbsoluteError (p->position, 0.0f, 0.01f);

            Knob words ("HP Filter", 0.0, 1.0, false, [] (double) { return juce::String ("Off"); });
            expect (! OwnLesson::placeAt (words, "hpf", 80.0).has_value());
        }

        beginTest ("steps add up from the player's setting; going back recomputes; @mine puts it back");
        {
            const auto r = LessonFile::parse (
                "id: own-t\ncourse: own\norder: 1\nminutes: 3\nplugin: any\ngroup: dynamics\nmaterial: loop\nauthor: t\n"
                "title.ru: Т\ntitle.en: T\n"
                "@step\n@knob threshold -20\nru: а\nen: a\n"
                "@step\n@knob ratio 4\n@bypass on\nru: б\nen: b\n"
                "@step\n@mine\n@bypass off\nru: в\nen: c\n",
                "t.lesson");
            expect (r.ok(), r.errors.joinIntoString ("\n"));

            Knob threshold ("Threshold", -60.0, 0.0, false, [] (double v) { return juce::String (v, 1) + " dB"; });
            Knob ratio ("Ratio", 1.0, 20.0, true, [] (double v) { return juce::String (v, 1) + ":1"; });
            juce::Array<juce::AudioProcessorParameter*> params { &threshold, &ratio };
            const OwnLesson::KnobMap map { { "threshold", 0 }, { "ratio", 1 } };
            const std::map<int, float> mine { { 0, 0.9f }, { 1, 0.1f } };

            expect (OwnLesson::fits (r.lesson, map));
            expect (! OwnLesson::fits (r.lesson, { { "threshold", 0 } }));

            const auto s1 = OwnLesson::stateAfter (r.lesson, 1, map, params, mine);
            expect (s1.dry);
            expectEquals ((int) s1.positions.size(), 2);
            expectWithinAbsoluteError (threshold.valueAt (s1.positions.at (0)), -20.0, 0.2);
            expectWithinRelativeError (ratio.valueAt (s1.positions.at (1)), 4.0, 0.05);

            const auto s0 = OwnLesson::stateAfter (r.lesson, 0, map, params, mine);
            expect (! s0.dry);
            expectEquals ((int) s0.positions.count (1), 0);

            const auto s2 = OwnLesson::stateAfter (r.lesson, 2, map, params, mine);
            expect (! s2.dry && s2.problems.isEmpty());
            expectEquals (s2.positions.at (0), 0.9f);
            expectEquals (s2.positions.at (1), 0.1f);
        }

        beginTest ("every shipped lesson for your own plugin lands on its numbers");
        {
            // One generous plugin with every glossary knob, each over the
            // lesson range of its unit and shown in that unit: whatever a
            // lesson asks for, it can reach - so a problem here is a typo
            // in a lesson, not a limit of a plugin.
            juce::OwnedArray<Knob> owned;
            juce::Array<juce::AudioProcessorParameter*> params;
            OwnLesson::KnobMap map;

            for (const auto& key : KnobGlossary::keys())
            {
                const auto unit = KnobGlossary::unitOf (key);
                const auto range = KnobGlossary::rangeOf (key);
                const auto logTaper = unit == "Hz" || unit == "ms" || unit == "s" || unit == "ratio" || unit == "Q";
                const auto lo = range.isEmpty() ? 0.0 : logTaper ? juce::jmax (0.01, range.getStart()) : range.getStart();
                const auto hi = range.isEmpty() ? 1.0 : range.getEnd();
                auto* k = owned.add (new Knob (key, lo, hi, logTaper, [unit] (double v)
                                               { return unit == "ratio" ? juce::String (v, 2) + ":1" : juce::String (v, 3) + " " + unit; }));
                map[key] = params.size();
                params.add (k);
            }

            std::map<int, float> mine;
            for (int i = 0; i < params.size(); ++i)
                mine[i] = 0.5f;

            for (const auto* l : LessonLibrary::forOwnPlugins())
            {
                expect (OwnLesson::fits (*l, map), l->id);
                const auto s = OwnLesson::stateAfter (*l, (int) l->steps.size() - 1, map, params, mine);
                for (const auto& p : s.problems)
                    expect (false, l->id + ": " + p);

                expect (l->knobKeys().size() >= 1, l->id + " turns no knob");
            }
        }
    }
};

static OwnLessonTest ownLessonTest;

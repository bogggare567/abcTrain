#include "OwnLesson.h"
#include "shared/learning/KnobGlossary.h"
#include <cmath>

namespace OwnLesson
{
    namespace
    {
        bool logScale (const juce::String& unit)
        {
            return unit == "Hz" || unit == "ms" || unit == "s" || unit == "ratio" || unit == "Q";
        }

        // How far a shown value is from the one asked for, in a measure
        // that means the same across the range: octaves-ish for times and
        // frequencies (10 ms vs 20 ms is as far as 100 vs 200), plain
        // difference for decibels and percent.
        double distance (const juce::String& unit, double shown, double target)
        {
            if (logScale (unit) && shown > 0.0 && target > 0.0)
                return std::abs (std::log2 (shown / target));
            return std::abs (shown - target);   // decibels, percent, and zero on a log knob (Pre-delay 0 ms)
        }

        bool closeEnough (const juce::String& unit, double shown, double target)
        {
            if (logScale (unit) && shown > 0.0 && target > 0.0)
                return std::abs (std::log2 (shown / target)) <= 0.15;   // ~10 %

            const auto tolerance = unit == "dB" ? 0.75 : unit == "%" ? 3.0 : unit == "ms" ? 0.5
                                 : unit == "Hz" ? 1.0 : unit == "s" ? 0.02 : 0.05;
            return std::abs (shown - target) <= tolerance;
        }

        juce::String withUnit (const juce::String& unit, double v)
        {
            const auto text = std::abs (v) >= 100.0 ? juce::String (juce::roundToInt (v)) : juce::String (v, 2);
            return unit == "ratio" ? text + ":1" : unit == "Q" ? text : text + " " + unit;
        }
    }

    std::optional<double> shownValue (const juce::String& textIn, const juce::String& label, const juce::String& key)
    {
        const auto unit = KnobGlossary::unitOf (key);
        auto t = textIn.trim().toLowerCase()
                     .replace (juce::String::fromUTF8 ("\xe2\x88\x92"), "-")   // U+2212 minus
                     .replace (juce::String::fromUTF8 ("\xc2\xb5"), "u")       // micro
                     .replace (",", ".");

        if (t.contains ("inf") || t.contains (juce::String::fromUTF8 ("\xe2\x88\x9e")))
            return t.startsWithChar ('-') ? -1000.0 : 1000.0;

        // The first number in the text.
        int start = -1;
        for (int i = 0; i < t.length(); ++i)
        {
            const juce::juce_wchar c = t[i];
            const juce::juce_wchar next = i + 1 < t.length() ? t[i + 1] : 0;
            if (juce::CharacterFunctions::isDigit (c)
                || ((c == '-' || c == '+' || c == '.') && juce::CharacterFunctions::isDigit (next)))
            {
                start = i;
                break;
            }
        }

        if (start < 0)
            return std::nullopt;

        int end = start + 1;
        while (end < t.length() && (juce::CharacterFunctions::isDigit (t[end]) || t[end] == '.'))
            ++end;

        auto value = t.substring (start, end).getDoubleValue();
        auto rest = t.substring (end).trim();
        if (rest.isEmpty())
            rest = label.trim().toLowerCase();

        // "4.0:1" is a ratio of 4; "1:4" (rare) is the same ratio written
        // the other way round.
        if (rest.startsWithChar (':'))
        {
            const auto other = rest.substring (1).trim().getDoubleValue();
            if (other > 0.0 && value > 0.0)
                value = value >= other ? value / other : other / value;
            return value;
        }

        if (unit == "Hz")
        {
            if (rest.startsWith ("k"))
                value *= 1000.0;
        }
        else if (unit == "ms")
        {
            if (rest.startsWith ("us"))
                value /= 1000.0;
            else if (rest.startsWith ("s") || rest.startsWith ("sec"))
                value *= 1000.0;
        }
        else if (unit == "s")
        {
            if (rest.startsWith ("ms"))
                value /= 1000.0;
            else if (rest.startsWith ("min"))
                value *= 60.0;
        }

        return value;
    }

    std::optional<Placement> placeAt (const juce::AudioProcessorParameter& p, const juce::String& key, double target)
    {
        const auto unit = KnobGlossary::unitOf (key);
        const auto label = p.getLabel();

        const auto shownAt = [&] (float pos) { return shownValue (p.getText (pos, 32), label, key); };

        // A coarse pass over the whole travel...
        constexpr int grid = 256;
        int best = -1;
        double bestDistance = 0.0, bestShown = 0.0;
        for (int i = 0; i <= grid; ++i)
        {
            const auto pos = (float) i / (float) grid;
            if (const auto v = shownAt (pos))
            {
                const auto d = distance (unit, *v, target);
                if (best < 0 || d < bestDistance)
                {
                    best = i;
                    bestDistance = d;
                    bestShown = *v;
                }
            }
        }

        if (best < 0)
            return std::nullopt;

        // ...then between the neighbours of the best point, assuming the
        // knob is monotonic over two grid cells (any real one is).
        auto lo = (float) juce::jmax (0, best - 1) / (float) grid;
        auto hi = (float) juce::jmin (grid, best + 1) / (float) grid;
        auto bestPos = (float) best / (float) grid;

        for (int i = 0; i < 24; ++i)
        {
            const auto a = lo + (hi - lo) / 3.0f, b = hi - (hi - lo) / 3.0f;
            const auto va = shownAt (a), vb = shownAt (b);
            const auto da = va ? distance (unit, *va, target) : 1.0e9;
            const auto db = vb ? distance (unit, *vb, target) : 1.0e9;
            if (da < bestDistance) { bestDistance = da; bestPos = a; bestShown = *va; }
            if (db < bestDistance) { bestDistance = db; bestPos = b; bestShown = *vb; }
            if (da < db) hi = b; else lo = a;
        }

        return Placement { juce::jlimit (0.0f, 1.0f, bestPos), closeEnough (unit, bestShown, target) };
    }

    bool fits (const LessonFile::Lesson& lesson, const KnobMap& knobs)
    {
        for (const auto& key : lesson.knobKeys())
            if (knobs.find (key) == knobs.end())
                return false;
        return lesson.forOwnPlugin();
    }

    State stateAfter (const LessonFile::Lesson& lesson, int upTo, const KnobMap& knobs,
                      const juce::Array<juce::AudioProcessorParameter*>& params, const std::map<int, float>& mine)
    {
        State s;

        const auto mineOf = [&mine] (int index)
        {
            const auto it = mine.find (index);
            return it != mine.end() ? it->second : 0.0f;
        };

        for (int i = 0; i <= upTo && i < (int) lesson.steps.size(); ++i)
            for (const auto& a : lesson.steps[(size_t) i].actions)
            {
                if (a.verb == "bypass")
                {
                    s.dry = a.args[0] == "on";
                }
                else if (a.verb == "mine")
                {
                    for (auto& [index, pos] : s.positions)
                        pos = mineOf (index);
                }
                else if (a.verb == "knob")
                {
                    const auto& key = a.args[0];
                    const auto it = knobs.find (key);
                    if (it == knobs.end())
                    {
                        s.problems.add (key + ": no such knob on this plugin");
                        continue;
                    }

                    const auto index = it->second;
                    auto* p = params[index];
                    if (p == nullptr)
                        continue;

                    float pos = mineOf (index);
                    if (a.has ("pos"))           pos = a.value ("pos", 0.5f);
                    else if (a.args[1] == "min")  pos = 0.0f;
                    else if (a.args[1] == "max")  pos = 1.0f;
                    else if (a.args[1] == "low")  pos = lowPosition;
                    else if (a.args[1] == "high") pos = highPosition;
                    else if (a.args[1] == "mine") pos = mineOf (index);
                    else
                    {
                        const auto target = a.args[1].getDoubleValue();
                        if (const auto placed = placeAt (*p, key, target))
                        {
                            pos = placed->position;
                            if (! placed->exact)
                            {
                                const auto unit = KnobGlossary::unitOf (key);
                                const auto shown = p->getText (pos, 32).trim();
                                s.problems.add (p->getName (64) + ": " + withUnit (unit, target)
                                                + " is out of reach, nearest " + shown);
                            }
                        }
                        else
                        {
                            s.problems.add (p->getName (64) + ": shows no number - left where it was");
                            continue;
                        }
                    }

                    s.positions[index] = juce::jlimit (0.0f, 1.0f, pos);
                }
            }

        return s;
    }
}

#include "LessonFile.h"

namespace LessonFile
{
    const juce::StringArray& courses()
    {
        static const juce::StringArray s { "eq", "comp", "verb", "routing", "channel", "vocal", "bus" };
        return s;
    }

    const juce::StringArray& plugins()
    {
        static const juce::StringArray s { "eq", "comp", "verb" };
        return s;
    }

    const juce::StringArray& materials()
    {
        static const juce::StringArray s { "vocal", "kick", "snare", "drums", "bass", "guitar",
                                           "keys", "mix", "pink", "chord", "hit" };
        return s;
    }

    const juce::StringArray& eqTypes()
    {
        static const juce::StringArray s { "bell", "lowshelf", "highshelf", "highpass", "lowpass", "notch" };
        return s;
    }

    const juce::StringArray& verbTypes()
    {
        static const juce::StringArray s { "room", "hall", "plate", "spring" };
        return s;
    }

    float Action::number (int argIndex, float fallback) const
    {
        return juce::isPositiveAndBelow (argIndex, args.size()) ? args[argIndex].getFloatValue() : fallback;
    }

    float Action::value (const juce::String& key, float fallback) const
    {
        return has (key) ? named[key].getFloatValue() : fallback;
    }

    juce::String Lesson::title (const juce::String& languageCode) const
    {
        return languageCode.startsWithIgnoreCase ("ru") ? titleRu : titleEn;
    }

    juce::String Lesson::text (const Step& step, const juce::String& languageCode) const
    {
        return languageCode.startsWithIgnoreCase ("ru") ? step.textRu : step.textEn;
    }

    namespace
    {
        bool isNumber (const juce::String& s)
        {
            return s.isNotEmpty() && s.containsOnly ("0123456789.-+");
        }

        // One command's arguments against the format's ranges. Each check
        // names the value and the range, so a guest author reading the
        // test output knows what to fix without opening this file.
        struct Checker
        {
            const juce::String& where;
            const Action& a;
            juce::StringArray& errors;

            void fail (const juce::String& what)
            {
                errors.add (where + ":" + juce::String (a.line) + ": @" + a.verb + ": " + what);
            }

            void range (const juce::String& label, const juce::String& text, float lo, float hi)
            {
                if (! isNumber (text))
                {
                    fail (label + " \"" + text + "\" is not a number");
                    return;
                }

                const auto v = text.getFloatValue();
                if (v < lo || v > hi)
                    fail (label + " " + text + " is outside " + juce::String (lo) + ".." + juce::String (hi));
            }

            void namedRanges (std::initializer_list<std::tuple<const char*, float, float>> allowed,
                              std::initializer_list<std::pair<const char*, juce::StringArray>> words = {})
            {
                if (a.named.size() == 0)
                    fail ("no parameters given");

                if (! a.args.isEmpty() && a.verb != "sidechain")
                    fail ("unexpected word \"" + a.args[0] + "\" (write key=value)");

                for (const auto& key : a.named.getAllKeys())
                {
                    bool known = false;

                    for (const auto& [name, lo, hi] : allowed)
                        if (key == name)
                        {
                            range (key, a.named[key], lo, hi);
                            known = true;
                        }

                    for (const auto& [name, options] : words)
                        if (key == name)
                        {
                            if (! options.contains (a.named[key]))
                                fail (key + " \"" + a.named[key] + "\" is not one of " + options.joinIntoString ("/"));
                            known = true;
                        }

                    if (! known)
                        fail ("unknown parameter \"" + key + "\"");
                }
            }
        };

        void check (const juce::String& where, const juce::String& plugin, const Action& a, juce::StringArray& errors)
        {
            Checker c { where, a, errors };
            const auto& v = a.verb;

            const auto needPlugin = [&] (const char* p)
            {
                if (plugin != p)
                    c.fail ("this command turns the " + juce::String (p) + " plugin but the lesson is plugin: " + plugin);
            };

            if (v == "eq-reset")
            {
                needPlugin ("eq");
                if (! a.args.isEmpty() || a.named.size() > 0)
                    c.fail ("takes no arguments");
            }
            else if (v == "eq")
            {
                needPlugin ("eq");
                if (a.args.size() < 3 || a.args.size() > 5 || a.named.size() > 0)
                {
                    c.fail ("write @eq <band 1-8> <type> <Hz> [dB] [Q]");
                    return;
                }
                c.range ("band", a.args[0], 1, 8);
                if (! eqTypes().contains (a.args[1]))
                    c.fail ("type \"" + a.args[1] + "\" is not one of " + eqTypes().joinIntoString ("/"));
                c.range ("frequency", a.args[2], 20, 20000);
                if (a.args.size() > 3) c.range ("gain", a.args[3], -18, 18);
                if (a.args.size() > 4) c.range ("Q", a.args[4], 0.1f, 18);
            }
            else if (v == "eq-off")
            {
                needPlugin ("eq");
                if (a.args.size() != 1)
                    c.fail ("write @eq-off <band 1-8>");
                else
                    c.range ("band", a.args[0], 1, 8);
            }
            else if (v == "comp")
            {
                needPlugin ("comp");
                c.namedRanges ({ { "threshold", -60, 0 }, { "ratio", 1, 20 }, { "attack", 0.1f, 100 },
                                 { "release", 10, 1000 }, { "knee", 0, 24 }, { "makeup", -24, 24 },
                                 { "mix", 0, 100 } });
            }
            else if (v == "sidechain")
            {
                needPlugin ("comp");
                if (a.args.size() != 1 || ! juce::StringArray { "self", "ext", "kick" }.contains (a.args[0]))
                    c.fail ("write @sidechain self|ext|kick [hpf=Hz|off]");

                for (const auto& key : a.named.getAllKeys())
                {
                    if (key != "hpf")
                        c.fail ("unknown parameter \"" + key + "\"");
                    else if (a.named[key] != "off")
                        c.range ("hpf", a.named[key], 20, 300);
                }
            }
            else if (v == "verb")
            {
                needPlugin ("verb");
                c.namedRanges ({ { "decay", 0.1f, 10 }, { "predelay", 0, 250 }, { "size", 0, 100 },
                                 { "damping", 0, 100 }, { "width", 0, 100 }, { "mix", 0, 100 } },
                               { { "type", verbTypes() }, { "routing", juce::StringArray { "insert", "send" } } });
            }
            else if (v == "bypass")
            {
                if (a.args.size() != 1 || ! juce::StringArray { "on", "off" }.contains (a.args[0]))
                    c.fail ("write @bypass on|off");
            }
            else if (v == "highlight")
            {
                if (a.args.size() != 2)
                {
                    c.fail ("write @highlight <from Hz> <to Hz>");
                    return;
                }
                c.range ("from", a.args[0], 20, 20000);
                c.range ("to", a.args[1], 20, 20000);
                if (a.number (0, 0) >= a.number (1, 0))
                    c.fail ("from must be below to");
            }
            else if (v == "material")
            {
                if (a.args.size() != 1 || ! materials().contains (a.args[0]))
                    c.fail ("write @material " + materials().joinIntoString ("|"));
            }
            else
            {
                c.fail ("unknown command");
            }
        }
    }

    Result parse (const juce::String& text, const juce::String& sourceName)
    {
        Result r;
        auto& lesson = r.lesson;
        auto& errors = r.errors;

        const auto fail = [&] (int line, const juce::String& what)
        {
            errors.add (sourceName + ":" + juce::String (line) + ": " + what);
        };

        juce::StringArray lines;
        lines.addLines (text);

        juce::StringPairArray header;
        Step* step = nullptr;
        juce::String* continuing = nullptr;   // the ru:/en: text a continuation line extends

        for (int i = 0; i < lines.size(); ++i)
        {
            const auto lineNo = i + 1;
            const auto raw = lines[i];
            const auto line = raw.trim();

            if (line.isEmpty() || line.startsWith ("#"))
            {
                continuing = nullptr;
                continue;
            }

            // An indented line goes on with the text above it.
            if (continuing != nullptr && (raw.startsWithChar (' ') || raw.startsWithChar ('\t')))
            {
                *continuing << " " << line;
                continue;
            }

            continuing = nullptr;

            if (line == "@step")
            {
                lesson.steps.push_back ({});
                step = &lesson.steps.back();
                step->line = lineNo;
                continue;
            }

            if (step == nullptr)
            {
                // Header: key: value.
                const auto colon = line.indexOfChar (':');
                if (colon <= 0)
                {
                    fail (lineNo, "expected \"key: value\" in the header, got \"" + line + "\"");
                    continue;
                }

                const auto key = line.substring (0, colon).trim().toLowerCase();
                if (header.containsKey (key))
                    fail (lineNo, "\"" + key + "\" given twice");
                header.set (key, line.substring (colon + 1).trim());
                continue;
            }

            if (line.startsWith ("ru:") || line.startsWith ("en:"))
            {
                auto& target = line.startsWith ("ru:") ? step->textRu : step->textEn;
                if (target.isNotEmpty())
                    fail (lineNo, "second " + line.substring (0, 2) + " text in one step");
                target = line.substring (3).trim();
                continuing = &target;
                continue;
            }

            if (line.startsWithChar ('@'))
            {
                Action a;
                a.line = lineNo;
                auto words = juce::StringArray::fromTokens (line.substring (1), " \t", "");
                words.removeEmptyStrings();
                a.verb = words[0].toLowerCase();

                for (int w = 1; w < words.size(); ++w)
                {
                    const auto& word = words[w];
                    const auto eq = word.indexOfChar ('=');
                    if (eq > 0)
                        a.named.set (word.substring (0, eq).toLowerCase(), word.substring (eq + 1).toLowerCase());
                    else
                        a.args.add (word.toLowerCase());
                }

                step->actions.push_back (a);
                continue;
            }

            fail (lineNo, "a line in a step must be a command (@...) or text (ru: / en:): \"" + line + "\"");
        }

        // Header.
        const auto need = [&] (const char* key) -> juce::String
        {
            const auto v = header[key];
            if (v.isEmpty())
                fail (1, juce::String ("missing header \"") + key + "\"");
            return v;
        };

        lesson.id = need ("id");
        lesson.course = need ("course");
        lesson.plugin = need ("plugin");
        lesson.material = need ("material");
        lesson.author = need ("author");
        lesson.titleRu = need ("title.ru");
        lesson.titleEn = need ("title.en");
        lesson.sources = header["sources"];
        lesson.order = need ("order").getIntValue();
        lesson.minutes = need ("minutes").getIntValue();

        for (const auto& key : header.getAllKeys())
            if (! juce::StringArray { "id", "course", "order", "minutes", "plugin", "material", "author",
                                      "sources", "title.ru", "title.en" }.contains (key))
                fail (1, "unknown header \"" + key + "\"");

        if (lesson.id.isNotEmpty() && ! lesson.id.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789-"))
            fail (1, "id \"" + lesson.id + "\" must be lower-case latin, digits and dashes");
        if (lesson.course.isNotEmpty() && ! courses().contains (lesson.course))
            fail (1, "course \"" + lesson.course + "\" is not one of " + courses().joinIntoString ("/"));
        if (lesson.plugin.isNotEmpty() && ! plugins().contains (lesson.plugin))
            fail (1, "plugin \"" + lesson.plugin + "\" is not one of " + plugins().joinIntoString ("/"));
        if (lesson.material.isNotEmpty() && ! materials().contains (lesson.material))
            fail (1, "material \"" + lesson.material + "\" is not one of " + materials().joinIntoString ("/"));
        if (header.containsKey ("minutes") && (lesson.minutes < 1 || lesson.minutes > 15))
            fail (1, "minutes " + juce::String (lesson.minutes) + " - a lesson is a few minutes, 1..15");
        if (header.containsKey ("order") && lesson.order < 1)
            fail (1, "order must be 1 or more");

        if (lesson.steps.empty())
            fail (1, "no @step");

        for (const auto& s : lesson.steps)
        {
            if (s.textRu.isEmpty())
                fail (s.line, "step without ru: text");
            if (s.textEn.isEmpty())
                fail (s.line, "step without en: text");

            for (const auto& a : s.actions)
                check (sourceName, lesson.plugin, a, errors);
        }

        return r;
    }
}

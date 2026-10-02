#include <juce_core/juce_core.h>
#include "../Source/LessonLibrary.h"
#include "../Source/LessonRunner.h"
#include "../LearnerEQ/Source/PluginProcessor.h"
#include "../LearnerComp/Source/PluginProcessor.h"
#include "../LearnerVerb/Source/PluginProcessor.h"
#include <set>

// The course lessons are text a guest author writes (lessons/FORMAT.md,
// ADR 052). This is the check that stands between a typo in one of those
// files and a lesson that silently does nothing: every shipped file
// parses, every command lands on a parameter the plugin really has, at a
// value inside its range.
class LessonFormatTest : public juce::UnitTest
{
public:
    LessonFormatTest() : juce::UnitTest ("LessonFormat", "Learning") {}

    void runTest() override
    {
        beginTest ("every shipped lesson parses, with nothing to fix");
        {
            const auto results = LessonLibrary::parseEverything();
            expect (results.size() >= 20, "lessons shipped: " + juce::String ((int) results.size()));

            for (const auto& r : results)
                for (const auto& e : r.errors)
                    expect (false, e);
        }

        beginTest ("ids are unique, every course has lessons, orders do not collide");
        {
            std::set<juce::String> ids;
            std::set<juce::String> orders;

            for (const auto& l : LessonLibrary::all())
            {
                expect (ids.insert (l.id).second, "id twice: " + l.id);
                expect (orders.insert (l.course + "#" + juce::String (l.order)).second,
                        "order twice in " + l.course + ": " + juce::String (l.order));
                expect (l.id.startsWith (l.course + "-"), "id should start with its course: " + l.id);
            }

            for (const auto& c : LessonFile::courses())
                expect (! std::none_of (LessonLibrary::all().begin(), LessonLibrary::all().end(),
                                        [&c] (const auto& l) { return l.course == c; }),
                        "no lesson in course " + c);
        }

        beginTest ("a lesson reads in the minutes it says: 3-5, about 120-220 words a minute");
        {
            for (const auto& l : LessonLibrary::all())
            {
                expect (l.minutes >= 3 && l.minutes <= 5, l.id + ": " + juce::String (l.minutes) + " min");
                expect (l.steps.size() >= 4 && l.steps.size() <= 10, l.id + ": " + juce::String ((int) l.steps.size()) + " steps");

                int words = 0;
                for (const auto& s : l.steps)
                    words += juce::StringArray::fromTokens (s.textRu, " ", "").size();

                expect (words <= l.minutes * 220, l.id + ": " + juce::String (words) + " words for "
                                                   + juce::String (l.minutes) + " min");
            }
        }

        beginTest ("every command lands on a real parameter, inside its range");
        {
            LearnerEQProcessor eq;
            LearnerCompProcessor comp;
            LearnerVerbProcessor verb;

            for (const auto& l : LessonLibrary::all())
            {
                auto& apvts = l.plugin == "eq" ? eq.apvts : l.plugin == "comp" ? comp.apvts : verb.apvts;

                for (const auto& s : l.steps)
                    for (const auto& [id, value] : LessonRunner::targetsFor (s, l.plugin))
                    {
                        auto* p = apvts.getParameter (id);
                        expect (p != nullptr, l.id + ": no parameter " + id);
                        if (p == nullptr)
                            continue;

                        const auto range = p->getNormalisableRange();
                        expect (value >= range.start - 1.0e-3f && value <= range.end + 1.0e-3f,
                                l.id + ": " + id + " = " + juce::String (value) + " outside "
                                    + juce::String (range.start) + ".." + juce::String (range.end));
                    }
            }
        }

        beginTest ("the parser names the line of each mistake");
        {
            const auto r = LessonFile::parse (
                "id: eq-test\ncourse: eq\norder: 1\nminutes: 4\nplugin: eq\nmaterial: vocal\nauthor: t\n"
                "title.ru: Т\ntitle.en: T\n\n"
                "@step\n@eq 1 bell 300 -40 1\n@eqq 2\nru: Текст.\n\n"
                "@step\n@comp ratio=4\nru: Только русский.\n",
                "bad.lesson");

            expect (! r.ok());
            const auto all = r.errors.joinIntoString ("\n");
            expect (all.contains ("bad.lesson:12") && all.contains ("gain"), all);
            expect (all.contains ("bad.lesson:13") && all.contains ("unknown command"), all);
            expect (all.contains ("without en"), all);
            expect (all.contains ("comp plugin"), all);
        }

        beginTest ("indented lines continue the text; directives map to the knobs");
        {
            const auto r = LessonFile::parse (
                "id: verb-t\ncourse: verb\norder: 9\nminutes: 3\nplugin: verb\nmaterial: hit\nauthor: t\n"
                "title.ru: Т\ntitle.en: T\n"
                "@step\n@verb type=plate decay=1.8 predelay=30 mix=25 routing=send\n"
                "ru: Первая строка\n    вторая строка.\nen: First line\n    second line.\n",
                "ok.lesson");

            expect (r.ok(), r.errors.joinIntoString ("\n"));
            expectEquals (r.lesson.steps[0].textRu, juce::String ("Первая строка вторая строка."));

            const auto t = LessonRunner::targetsFor (r.lesson.steps[0], "verb");
            const auto find = [&t] (const char* id) { for (const auto& [k, v] : t) if (k == id) return v; return -999.0f; };
            expectEquals (find (LearnerVerbProcessor::typeParamId), 2.0f);
            expectEquals (find (LearnerVerbProcessor::routingParamId), (float) LearnerVerbProcessor::send);
            expectEquals (find (LearnerVerbProcessor::preDelayParamId), 30.0f);
            expectEquals (find (LearnerVerbProcessor::dryWetParamId), 25.0f);
        }

        beginTest ("the knobs glide to the step's values and end exactly there");
        {
            LearnerCompProcessor comp;
            LessonRunner runner;
            runner.apply (comp.apvts, { { LearnerCompProcessor::thresholdParamId, -24.0f },
                                        { LearnerCompProcessor::scSourceParamId, (float) LearnerCompProcessor::scKick } });

            // A choice does not glide; a float does, until finished.
            expectEquals ((int) comp.apvts.getRawParameterValue (LearnerCompProcessor::scSourceParamId)->load(),
                          (int) LearnerCompProcessor::scKick);
            expect (runner.isGliding());
            runner.finish();
            expectWithinAbsoluteError (comp.apvts.getRawParameterValue (LearnerCompProcessor::thresholdParamId)->load(), -24.0f, 0.51f);
        }
    }
};

static LessonFormatTest lessonFormatTest;

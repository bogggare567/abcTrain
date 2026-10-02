#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// A lesson written as a plain text file (lessons/FORMAT.md, ADR 052).
//
// The lessons inside the Learner plugins are C++ - fine for the three or
// four walkthroughs each plugin carries, hopeless for a course somebody
// else writes: a sound engineer invited to add a lesson on vocal chains
// should not need a compiler. So a course lesson is a text file with a
// header, steps, and one-line commands that say where the knobs go. This
// file only reads and checks that text; turning the commands into
// parameter moves is the app's job (Source/LessonRunner), because only
// the app knows the three plugins' parameter ids.
//
// Pure and synchronous: no files, no GUI, no audio. Everything a lesson
// can get wrong - an unknown command, a gain of +40 dB, a step with no
// English - comes back as an error naming the line, and
// tests/LessonFormatTest reads every shipped lesson through it.
namespace LessonFile
{
    // One command line ("@eq 1 bell 300 -4 1.4", "@comp ratio=4 attack=10").
    struct Action
    {
        juce::String verb;                  // "eq", "eq-off", "comp", "verb", "bypass" ...
        juce::StringArray args;             // positional words, in order
        juce::StringPairArray named;        // key=value words, keys lower-case
        int line = 0;

        float number (int argIndex, float fallback) const;
        bool has (const juce::String& key) const { return named.containsKey (key); }
        float value (const juce::String& key, float fallback) const;
    };

    struct Step
    {
        juce::String textRu, textEn;
        std::vector<Action> actions;
        int line = 0;
    };

    struct Lesson
    {
        juce::String id, course, plugin, material, author, sources;
        juce::String titleRu, titleEn;
        int order = 0, minutes = 0;
        std::vector<Step> steps;

        // Russian for a Russian interface, English for every other: the
        // two languages every lesson must carry (FORMAT.md). A missing
        // translation would read worse than an honest English one.
        juce::String title (const juce::String& languageCode) const;
        juce::String text (const Step&, const juce::String& languageCode) const;
    };

    struct Result
    {
        Lesson lesson;
        juce::StringArray errors;           // "name:12: unknown command @eqq"
        bool ok() const noexcept { return errors.isEmpty(); }
    };

    Result parse (const juce::String& text, const juce::String& sourceName);

    // The value sets the format allows, for the parser and for the UI.
    const juce::StringArray& courses();     // eq comp verb routing channel vocal bus
    const juce::StringArray& plugins();     // eq comp verb
    const juce::StringArray& materials();   // vocal kick snare drums bass guitar keys mix pink chord hit
    const juce::StringArray& eqTypes();     // bell lowshelf highshelf highpass lowpass notch
    const juce::StringArray& verbTypes();   // room hall plate spring
}

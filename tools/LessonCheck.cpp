// LessonCheck - reads course lessons the way the app will and says what is
// wrong with each, file and line (lessons/CONTRIBUTING.md, ADR 070).
//
//     LessonCheck                      every lesson under ./lessons
//     LessonCheck lessons/own/my.lesson lessons/eq
//
// A guest author runs it before opening a pull request; the "Lessons"
// workflow runs it on every pull request that touches lessons/ and turns
// each problem into a note on the line in GitHub. The same rules as
// tests/LessonFormatTest, from the same code (LessonFile, OwnLesson), so
// the two can never disagree. What it cannot check without building the
// plugins - that a Studio lesson's value is inside the Learner knob's
// range - the full CI build does.
//
// Exit code: 0 if every file is fine, 1 otherwise.

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "shared/learning/LessonFile.h"
#include "../Source/OwnPlugins/OwnLesson.h"
#include <iostream>
#include <map>

namespace
{
    bool inGitHub() { return juce::SystemStats::getEnvironmentVariable ("GITHUB_ACTIONS", {}) == "true"; }

    // "lessons/own/x.lesson:12: message" -> a GitHub annotation on that line.
    void report (const juce::String& problem, const juce::String& file)
    {
        std::cout << problem << std::endl;
        if (! inGitHub())
            return;

        int line = 1;
        auto message = problem;
        const auto afterFile = problem.fromFirstOccurrenceOf (file + ":", false, false);
        if (afterFile.isNotEmpty() && juce::CharacterFunctions::isDigit (afterFile[0]))
        {
            line = afterFile.getIntValue();
            message = afterFile.fromFirstOccurrenceOf (":", false, false).trim();
        }
        std::cout << "::error file=" << file << ",line=" << line << "::" << message.replace ("\n", " ") << std::endl;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;   // juce_audio_processors wants a message manager

    const auto cwd = juce::File::getCurrentWorkingDirectory();
    juce::Array<juce::File> files;

    const auto addFrom = [&files, &cwd] (const juce::String& arg)
    {
        const auto f = cwd.getChildFile (arg);
        if (f.isDirectory())
            files.addArray (f.findChildFiles (juce::File::findFiles, true, "*.lesson"));
        else if (f.existsAsFile())
            files.add (f);
        else
            std::cout << arg << ": no such file or folder" << std::endl;
    };

    if (argc < 2)
        addFrom ("lessons");
    for (int i = 1; i < argc; ++i)
        addFrom (argv[i]);

    // Ids and orders are checked against every lesson in the repository,
    // not only the files given: a new lesson must not take an existing id.
    std::map<juce::String, juce::String> idOwner, orderOwner;
    const auto root = cwd.getChildFile ("lessons");
    if (root.isDirectory())
        for (const auto& f : root.findChildFiles (juce::File::findFiles, true, "*.lesson"))
            if (! files.contains (f))
            {
                const auto r = LessonFile::parse (f.loadFileAsString(), f.getRelativePathFrom (cwd));
                idOwner[r.lesson.id] = f.getRelativePathFrom (cwd);
                orderOwner[r.lesson.course + "#" + juce::String (r.lesson.order)] = f.getRelativePathFrom (cwd);
            }

    int bad = 0;
    for (const auto& f : files)
    {
        const auto name = f.getRelativePathFrom (cwd).replaceCharacter ('\\', '/');
        const auto r = LessonFile::parse (f.loadFileAsString(), name);

        juce::StringArray problems (r.errors);
        if (r.ok())
        {
            problems.addArray (LessonFile::styleProblems (r.lesson));
            problems.addArray (OwnLesson::glossaryProblems (r.lesson));

            const auto& l = r.lesson;
            if (! l.id.startsWith (l.course + "-"))
                problems.add (name + ":1: id \"" + l.id + "\" should start with its course: " + l.course + "-");
            if (f.getParentDirectory().getFileName() != l.course)
                problems.add (name + ":1: course " + l.course + " - put the file in lessons/" + l.course + "/");

            const auto orderKey = l.course + "#" + juce::String (l.order);
            if (idOwner.count (l.id) > 0)
                problems.add (name + ":1: id \"" + l.id + "\" is already taken by " + idOwner[l.id]);
            if (orderOwner.count (orderKey) > 0)
                problems.add (name + ":1: order " + juce::String (l.order) + " in course " + l.course
                              + " is already taken by " + orderOwner[orderKey]);
            idOwner[l.id] = name;
            orderOwner[orderKey] = name;
        }

        if (problems.isEmpty())
        {
            std::cout << "ok   " << name << "  (" << (int) r.lesson.steps.size() << " steps, "
                      << r.lesson.minutes << " min, " << r.lesson.author << ")" << std::endl;
            continue;
        }

        ++bad;
        for (const auto& p : problems)
            report (p, name);
    }

    std::cout << std::endl << files.size() - bad << " of " << files.size() << " lesson(s) fine" << std::endl;
    return bad == 0 && ! files.isEmpty() ? 0 : 1;
}

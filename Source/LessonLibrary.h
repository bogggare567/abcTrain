#pragma once

#include "shared/learning/LessonFile.h"
#include <vector>

// The course lessons that ship inside the app (lessons/*/*.lesson,
// embedded as LessonData - see CMakeLists.txt and ADR 052), parsed once.
//
// A lesson that fails to parse is left out rather than shown half-broken;
// tests/LessonFormatTest is what keeps that from ever happening to a
// shipped one.
namespace LessonLibrary
{
    struct Course
    {
        juce::String id;                              // "eq", "comp", ...
        std::vector<const LessonFile::Lesson*> lessons;   // by `order`
    };

    // Every shipped lesson, by course (in LessonFile::courses() order) and
    // then by `order`.
    const std::vector<LessonFile::Lesson>& all();
    std::vector<Course> courses();
    const LessonFile::Lesson* find (const juce::String& id);

    // Every embedded file, parsed, errors included - for the test.
    std::vector<LessonFile::Result> parseEverything();
}

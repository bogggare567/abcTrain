#include "LessonLibrary.h"
#include "LessonData.h"
#include <algorithm>

namespace LessonLibrary
{
    std::vector<LessonFile::Result> parseEverything()
    {
        std::vector<LessonFile::Result> out;

        for (int i = 0; i < LessonData::namedResourceListSize; ++i)
        {
            const auto* name = LessonData::namedResourceList[i];
            const auto* file = LessonData::originalFilenames[i];
            int size = 0;
            const auto* data = LessonData::getNamedResource (name, size);

            if (data == nullptr || ! juce::String (file).endsWith (".lesson"))
                continue;

            out.push_back (LessonFile::parse (juce::String::fromUTF8 (data, size), file));
        }

        return out;
    }

    const std::vector<LessonFile::Lesson>& all()
    {
        static const std::vector<LessonFile::Lesson> lessons = []
        {
            std::vector<LessonFile::Lesson> ok;

            for (auto& r : parseEverything())
                if (r.ok())
                    ok.push_back (std::move (r.lesson));

            const auto& order = LessonFile::courses();
            std::sort (ok.begin(), ok.end(), [&order] (const auto& a, const auto& b)
            {
                const auto ca = order.indexOf (a.course), cb = order.indexOf (b.course);
                return ca != cb ? ca < cb : a.order < b.order;
            });
            return ok;
        }();

        return lessons;
    }

    std::vector<Course> courses()
    {
        std::vector<Course> out;

        for (const auto& id : LessonFile::courses())
        {
            Course c { id, {} };
            for (const auto& l : all())
                if (l.course == id)
                    c.lessons.push_back (&l);

            if (! c.lessons.empty())
                out.push_back (std::move (c));
        }

        return out;
    }

    const LessonFile::Lesson* find (const juce::String& id)
    {
        for (const auto& l : all())
            if (l.id == id)
                return &l;
        return nullptr;
    }
}

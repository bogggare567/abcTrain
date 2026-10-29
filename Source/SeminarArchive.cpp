#include "SeminarArchive.h"
#include <algorithm>

int SeminarArchive::Session::answeredTotal() const
{
    int n = 0;
    for (const auto& r : rounds)
        n += r.answered;
    return n;
}

int SeminarArchive::Session::rightTotal() const
{
    int n = 0;
    for (const auto& r : rounds)
        n += r.right;
    return n;
}

juce::File SeminarArchive::folder()
{
    // Next to the settings file (LocalisationManager::makeDefaultOptions).
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support/abcTrain/seminars");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("abcTrain/seminars");
   #endif
}

juce::var SeminarArchive::toVar (const Session& s)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("title", s.title);
    o->setProperty ("when", s.when.toMilliseconds());
    o->setProperty ("anonymous", s.anonymous);
    o->setProperty ("people", s.people);

    juce::Array<juce::var> rounds;
    for (const auto& r : s.rounds)
    {
        auto* ro = new juce::DynamicObject();
        ro->setProperty ("game", r.game);
        ro->setProperty ("exercise", r.exercise);
        ro->setProperty ("answer", r.answer);
        ro->setProperty ("sound", r.sound);
        ro->setProperty ("answered", r.answered);
        ro->setProperty ("right", r.right);
        juce::Array<juce::var> missed;
        for (const auto& m : r.missed)
            missed.add (m);
        ro->setProperty ("missed", missed);
        rounds.add (juce::var (ro));
    }

    o->setProperty ("rounds", rounds);
    o->setProperty ("version", 1);
    return juce::var (o);
}

SeminarArchive::Session SeminarArchive::fromVar (const juce::var& v)
{
    Session s;
    s.title = v.getProperty ("title", "").toString();
    s.when = juce::Time ((juce::int64) v.getProperty ("when", 0));
    s.anonymous = (bool) v.getProperty ("anonymous", false);
    s.people = (int) v.getProperty ("people", 0);

    if (auto* rounds = v.getProperty ("rounds", {}).getArray())
        for (const auto& rv : *rounds)
        {
            Round r;
            r.game = (int) rv.getProperty ("game", -1);
            r.exercise = rv.getProperty ("exercise", "").toString();
            r.answer = rv.getProperty ("answer", "").toString();
            r.sound = rv.getProperty ("sound", "").toString();
            r.answered = (int) rv.getProperty ("answered", 0);
            r.right = (int) rv.getProperty ("right", 0);
            if (auto* missed = rv.getProperty ("missed", {}).getArray())
                for (const auto& m : *missed)
                    r.missed.add (m.toString());
            s.rounds.push_back (r);
        }

    return s;
}

juce::File SeminarArchive::save (const Session& s)
{
    auto dir = folder();
    if (! dir.createDirectory())
        return {};

    auto file = dir.getNonexistentChildFile (s.when.formatted ("%Y-%m-%d_%H-%M"), ".json", false);
    return file.replaceWithText (juce::JSON::toString (toVar (s))) ? file : juce::File();
}

std::vector<SeminarArchive::Session> SeminarArchive::list()
{
    std::vector<Session> sessions;

    for (const auto& file : folder().findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        const auto parsed = juce::JSON::parse (file);
        if (! parsed.isObject())
            continue;

        auto s = fromVar (parsed);
        s.file = file;
        sessions.push_back (std::move (s));
    }

    std::sort (sessions.begin(), sessions.end(), [] (const Session& a, const Session& b) { return a.when > b.when; });
    return sessions;
}

juce::String SeminarArchive::toCsv (const Session& s)
{
    const auto cell = [] (juce::String t)
    {
        if (t.containsAnyOf (",\"\n;"))
            t = "\"" + t.replace ("\"", "\"\"") + "\"";
        return t;
    };

    juce::String csv;
    csv << "round,exercise,answer,sound,answered,right,missed\n";

    for (size_t i = 0; i < s.rounds.size(); ++i)
    {
        const auto& r = s.rounds[i];
        csv << juce::String ((int) i + 1) << "," << cell (r.exercise) << "," << cell (r.answer) << ","
            << cell (r.sound) << "," << r.answered << "," << r.right << ","
            << cell (r.missed.joinIntoString ("; ")) << "\n";
    }

    return csv;
}

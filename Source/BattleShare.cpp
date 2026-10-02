#include "BattleShare.h"
#include "shared/ui/CommunityLink.h"

namespace BattleShare
{
    static juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

    juce::String animalOf (const juce::String& botId)
    {
        if (botId == "hound")    return u8 ("\xf0\x9f\x90\x95");   // dog
        if (botId == "cat")      return u8 ("\xf0\x9f\x90\xb1");
        if (botId == "viper")    return u8 ("\xf0\x9f\x90\x8d");
        if (botId == "owl")      return u8 ("\xf0\x9f\xa6\x89");
        if (botId == "bat")      return u8 ("\xf0\x9f\xa6\x87");
        if (botId == "elephant") return u8 ("\xf0\x9f\x90\x98");
        return {};
    }

    juce::String format (const Result& r, const Words& w)
    {
        juce::String s;
        s << u8 ("\xf0\x9f\x8e\xa7 ") << w.title;
        if (r.exercise.isNotEmpty())
            s << u8 (" \xc2\xb7 ") << r.exercise;
        s << "\n\n";

        const auto them = r.themBot.isNotEmpty() ? animalOf (r.themBot) + " " + r.them
                                                 : u8 ("\xf0\x9f\x91\xa4 ") + r.them;
        s << u8 ("\xf0\x9f\x91\xa4 ") << r.you << " " << w.vs << " " << them << "\n";

        switch (r.outcome)
        {
            case Outcome::won:  s << u8 ("\xf0\x9f\x8f\x86 ") << w.won;  break;
            case Outcome::lost: s << u8 ("\xf0\x9f\x92\x80 ") << w.lost; break;
            case Outcome::draw: s << u8 ("\xf0\x9f\xa4\x9d ") << w.draw; break;
        }
        s << "\n\n";

        s << u8 ("\xe2\x9d\xa4\xef\xb8\x8f ") << r.hpYou << " HP " << w.vs << " " << r.hpThem << " HP\n";
        s << u8 ("\xf0\x9f\x8e\xaf ") << r.scoreYou << " : " << r.scoreThem;
        if (r.rounds.isNotEmpty())
            s << u8 (" \xc2\xb7 ") << r.rounds;
        s << "\n";

        for (const auto& n : r.notes)
            if (n.trim().isNotEmpty())
                s << u8 ("\xf0\x9f\x93\x8a ") << n.trim() << "\n";

        s << "\n" << CommunityLink::shortUrl;
        return s;
    }
}

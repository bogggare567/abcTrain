#include "PackCatalog.h"
#include <juce_events/juce_events.h>

namespace PackCatalog
{
    std::vector<Entry> parse (const juce::String& json)
    {
        std::vector<Entry> out;
        const auto releases = juce::JSON::parse (json);
        if (! releases.isArray())
            return out;

        for (const auto& r : *releases.getArray())
        {
            if ((bool) r.getProperty ("draft", false))
                continue;
            const auto title = r.getProperty ("name", r.getProperty ("tag_name", "")).toString();
            if (const auto* assets = r.getProperty ("assets", {}).getArray())
                for (const auto& a : *assets)
                {
                    const auto name = a.getProperty ("name", "").toString();
                    if (! name.endsWithIgnoreCase (".zip"))
                        continue;
                    out.push_back ({ name, title, a.getProperty ("browser_download_url", "").toString(),
                                     (juce::int64) a.getProperty ("size", 0) });
                }
        }
        return out;
    }

    void fetch (std::function<void (std::vector<Entry>, juce::String)> done)
    {
        juce::Thread::launch ([done]
        {
            std::vector<Entry> list;
            juce::String error;
            int status = 0;
            auto stream = juce::URL (releasesUrl).createInputStream (
                juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs (8000)
                    .withExtraHeaders ("Accept: application/vnd.github+json\r\nUser-Agent: abcTrain")
                    .withStatusCode (&status));

            if (stream == nullptr)
                error = "offline";
            else if (status == 404)
                error = "none";
            else
                list = parse (stream->readEntireStreamAsString());

            juce::MessageManager::callAsync ([done, list, error] { done (list, error); });
        });
    }

    void download (const Entry& e, std::function<void (juce::File)> done)
    {
        const auto url = e.url;
        const auto name = e.name;
        juce::Thread::launch ([done, url, name]
        {
            const auto target = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile (
                name.upToLastOccurrenceOf (".", false, false), ".zip");
            auto stream = juce::URL (url).createInputStream (
                juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs (15000)
                    .withExtraHeaders ("User-Agent: abcTrain"));
            bool ok = false;
            if (stream != nullptr)
            {
                juce::FileOutputStream out (target);
                ok = out.openedOk() && out.writeFromInputStream (*stream, -1) > 0;
            }
            const auto result = ok ? target : juce::File();
            juce::MessageManager::callAsync ([done, result] { done (result); });
        });
    }
}

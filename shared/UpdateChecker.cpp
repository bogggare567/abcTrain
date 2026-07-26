#include "UpdateChecker.h"
#include <juce_events/juce_events.h>
#include <vector>

namespace UpdateChecker
{
    namespace
    {
        constexpr const char* latestReleaseApiUrl = "https://api.github.com/repos/bogggare567/abcTrain/releases/latest";
        constexpr const char* releaseListApiUrl = "https://api.github.com/repos/bogggare567/abcTrain/releases";

        // Empty (not {0}) means "didn't parse" - lets isNewerVersion tell
        // "malformed" apart from "version 0".
        std::vector<int> parseVersionComponents (const juce::String& version)
        {
            auto trimmed = version.trim();
            if (trimmed.startsWithIgnoreCase ("v"))
                trimmed = trimmed.substring (1);

            if (trimmed.isEmpty())
                return {};

            // Everything from the first pre-release/build separator on is
            // metadata, not part of the version being compared: git
            // describe hands us "v1.0.0-3-gabc123" for any build that
            // isn't exactly on a tag, and "v1.0.0-beta" for a pre-release.
            //
            // This used to reject both outright, which meant *every* build
            // between two tags silently reported "up to date" forever -
            // the update check was dead for anyone building from source.
            const auto separator = trimmed.indexOfAnyOf ("-+");
            if (separator > 0)
                trimmed = trimmed.substring (0, separator);

            std::vector<int> components;
            for (auto& part : juce::StringArray::fromTokens (trimmed, ".", ""))
            {
                if (part.isEmpty() || ! part.containsOnly ("0123456789"))
                    return {};

                components.push_back (part.getIntValue());
            }

            return components;
        }
    }

    bool isNewerVersion (const juce::String& latest, const juce::String& current) noexcept
    {
        const auto latestParts = parseVersionComponents (latest);
        const auto currentParts = parseVersionComponents (current);

        if (latestParts.empty() || currentParts.empty())
            return false;

        const auto numComponents = juce::jmax (latestParts.size(), currentParts.size());

        for (size_t i = 0; i < numComponents; ++i)
        {
            const auto l = i < latestParts.size() ? latestParts[i] : 0;
            const auto c = i < currentParts.size() ? currentParts[i] : 0;

            if (l != c)
                return l > c;
        }

        return false; // equal versions - not "newer"
    }

    ReleaseInfo parseReleaseJson (const juce::String& json)
    {
        // `parsed` must outlive `obj`: var::getDynamicObject() returns a
        // raw pointer into the var's ref-counted DynamicObject, so calling
        // it on a temporary (juce::JSON::parse(json).getDynamicObject())
        // would leave `obj` dangling the moment that temporary is
        // destroyed at the end of the full expression.
        const auto parsed = juce::JSON::parse (json);

        if (auto* obj = parsed.getDynamicObject())
        {
            ReleaseInfo info;
            info.tagName = obj->getProperty ("tag_name").toString();
            info.htmlUrl = obj->getProperty ("html_url").toString();
            return info;
        }

        return {};
    }

    ReleaseInfo parseReleaseListJson (const juce::String& json, bool allowPrerelease)
    {
        const auto parsed = juce::JSON::parse (json);

        if (auto* array = parsed.getArray())
        {
            for (auto& entry : *array)
            {
                if (auto* obj = entry.getDynamicObject())
                {
                    const auto isPrerelease = (bool) obj->getProperty ("prerelease");
                    if (allowPrerelease || ! isPrerelease)
                    {
                        ReleaseInfo info;
                        info.tagName = obj->getProperty ("tag_name").toString();
                        info.htmlUrl = obj->getProperty ("html_url").toString();
                        return info;
                    }
                }
            }
        }

        return {};
    }

    void checkForUpdatesAsync (const juce::String& currentVersion, Channel channel,
                                std::function<void (bool, ReleaseInfo)> callback)
    {
        juce::Thread::launch ([currentVersion, channel, callback]
        {
            const juce::URL url (channel == Channel::beta ? releaseListApiUrl : latestReleaseApiUrl);

            auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                      .withConnectionTimeoutMs (5000)
                                                      .withExtraHeaders ("Accept: application/vnd.github+json"));
            if (stream == nullptr)
                return;

            const auto body = stream->readEntireStreamAsString();
            const auto release = channel == Channel::beta
                                      ? parseReleaseListJson (body, true)
                                      : parseReleaseJson (body);

            if (release.tagName.isEmpty())
                return;

            const auto foundNewer = isNewerVersion (release.tagName, currentVersion);

            juce::MessageManager::callAsync ([callback, foundNewer, release]
            {
                callback (foundNewer, release);
            });
        });
    }
}

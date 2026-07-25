#pragma once

#include <juce_core/juce_core.h>
#include <regex>

// Turns a "vX.Y.Z"-shaped version/describe string into a release channel.
// Pure function, no CMake/build dependency - takes any string (in
// practice CurrentVersion::string, i.e. VersionInfo::versionString, but
// tests exercise it directly with hand-written examples) so it's
// testable without needing a real git-tagged build to check against.
// Uses std::regex rather than a JUCE equivalent - JUCE doesn't ship a
// general-purpose regex class, unlike e.g. String::matchesWildcard()
// which only does glob patterns.
namespace VersionChannel
{
    enum class Channel
    {
        stable, // an exact "vX.Y.Z" tag (or "X.Y.Z", no leading v)
        beta,   // contains "-beta" (case-insensitive), e.g. "v1.0.0-beta1"
        dev     // anything else: N-commits-past-a-tag ("-N-gHASH"), a
                // dirty tree ("-dirty"), or no tag reachable at all
                // ("0.0.0-dev+sha1234567")
    };

    inline Channel detect (const juce::String& version) noexcept
    {
        if (version.containsIgnoreCase ("-beta"))
            return Channel::beta;

        // An exact release tag has nothing after the patch number at
        // all - "v1.0.0", not "v1.0.0-5-g1a2b3c4" or "v1.0.0-dirty".
        static const std::regex stablePattern (R"(^v?[0-9]+\.[0-9]+\.[0-9]+$)");
        if (std::regex_match (version.toStdString(), stablePattern))
            return Channel::stable;

        return Channel::dev;
    }

    inline juce::String toString (Channel channel)
    {
        switch (channel)
        {
            case Channel::stable: return "stable";
            case Channel::beta:   return "beta";
            case Channel::dev:    return "dev";
        }
        return "dev";
    }
}

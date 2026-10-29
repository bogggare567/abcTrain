#pragma once

#include "shared/VersionInfo.h"

// The version string each plugin's "Check for Updates" button compares
// against GitHub's latest release tag via UpdateChecker::isNewerVersion.
//
// Sourced from `git describe --tags --dirty --always` at CMake configure
// time (see the "Version from git describe" block in the root
// CMakeLists.txt), not a hand-bumped literal - shared/VersionInfo.h is
// generated into the build directory, never committed. Still just a
// plain `constexpr const char*` here rather than JucePlugin_VersionString
// directly: the three Learner plugin editors are also compiled directly
// into EarTrainerTests (see CMakeLists.txt), where JucePlugin_* macros
// aren't defined - the same reason LearnerEQProcessor::getName() returns
// a literal instead of JucePlugin_Name (see docs/diagrams/ci-pipeline.md,
// bug 1).
//
// See shared/updates/VersionChannel.h for turning this (or any "vX.Y.Z"-shaped
// string) into a stable/beta/dev channel.
namespace CurrentVersion
{
    constexpr const char* string = VersionInfo::versionString;

    // "Saint Petersburg" for 2.0.x (shared/updates/release-names.txt).
    constexpr const char* codename = VersionInfo::codename;

    // Each part's own version (versions.json, ADR 069). The app's is
    // `string` above (the release tag); the plugins and the lessons move
    // only when they change.
    namespace Part
    {
        inline constexpr const char* learnerEQ   = "learner-eq";
        inline constexpr const char* learnerComp = "learner-comp";
        inline constexpr const char* learnerVerb = "learner-verb";
        inline constexpr const char* lessons     = "lessons";
    }

    /** The version of a part by its versions.json key; the app's for an
        empty or unknown key. */
    inline const char* of (const char* part) noexcept
    {
        const auto is = [part] (const char* key)
        {
            if (part == nullptr)
                return false;
            for (auto* a = part, *b = key;; ++a, ++b)
            {
                if (*a != *b) return false;
                if (*a == 0)  return true;
            }
        };

        if (is (Part::learnerEQ))   return VersionInfo::learnerEQ;
        if (is (Part::learnerComp)) return VersionInfo::learnerComp;
        if (is (Part::learnerVerb)) return VersionInfo::learnerVerb;
        if (is (Part::lessons))     return VersionInfo::lessons;
        return string;
    }
}

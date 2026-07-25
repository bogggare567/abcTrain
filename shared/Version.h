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
// See shared/VersionChannel.h for turning this (or any "vX.Y.Z"-shaped
// string) into a stable/beta/dev channel.
namespace CurrentVersion
{
    constexpr const char* string = VersionInfo::versionString;
}

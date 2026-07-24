#pragma once

// The version string each plugin's "Check for Updates" button compares
// against GitHub's latest release tag via UpdateChecker::isNewerVersion.
//
// Deliberately a plain literal, not JucePlugin_VersionString: the three
// Learner plugin editors are also compiled directly into EarTrainerTests
// (see CMakeLists.txt), where JucePlugin_* macros aren't defined - the
// same reason LearnerEQProcessor::getName() returns a literal instead of
// JucePlugin_Name (see docs/diagrams/ci-pipeline.md, bug 1).
//
// Bump this by hand alongside `project(EarTrainer VERSION ...)` in
// CMakeLists.txt and whatever git tag gets pushed for a release.
namespace CurrentVersion
{
    constexpr const char* string = "0.1.0";
}

cmake_minimum_required(VERSION 3.22)

# Link the MSVC runtime statically on Windows.
#
# A reported failure on a real user's machine: "VCRUNTIME140_1.dll was not
# found". The default /MD build makes every binary depend on the Visual
# C++ Redistributable being installed - which it is on a developer's
# machine and on a GitHub Actions runner, and often is not on a listener's
# PC, an older machine, or a fresh install. The person then has to know
# what a redistributable is and go and find it, which is not a thing to
# ask of someone who just wanted to try an ear trainer.
#
# Bundling vc_redist.exe with the installer would also work, but it needs
# admin rights, adds a reboot prompt in some cases, and does nothing for
# anyone who unzips the portable build instead of running the installer.
# Static linking removes the dependency entirely: the binaries carry what
# they need. The cost is a few hundred KB per binary, which against a
# plugin that already embeds JUCE is nothing.
#
# Must be set *before* project(), and needs CMP0091 (CMake >= 3.15) so
# CMake manages the runtime flag itself instead of it being baked into
# CMAKE_CXX_FLAGS - hence the explicit policy line rather than relying on
# the 3.22 minimum above to set it.
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    CACHE STRING "MSVC runtime library (static, so no redistributable is needed)")

project(EarTrainer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Version from git describe, not a hand-bumped literal. `--tags --dirty
# --always` yields "vX.Y.Z" on an exact tag, "vX.Y.Z-N-gHASH[-dirty]" N
# commits past one, or (with --always, no tag reachable at all in this
# clone's history) a bare short hash - the regex below only catches the
# "no tag reachable" case and turns that bare hash into the spec'd
# "0.0.0-dev+sha1234567" form; the other two shapes are already a real,
# usable version string as-is.
find_package(Git QUIET)
set(ABCTRAIN_GIT_DESCRIBE "unknown")
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE ABCTRAIN_GIT_DESCRIBE_RAW
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE ABCTRAIN_GIT_DESCRIBE_RESULT
        ERROR_QUIET
    )
    if(ABCTRAIN_GIT_DESCRIBE_RESULT EQUAL 0 AND ABCTRAIN_GIT_DESCRIBE_RAW)
        set(ABCTRAIN_GIT_DESCRIBE ${ABCTRAIN_GIT_DESCRIBE_RAW})
    endif()
endif()

if(ABCTRAIN_GIT_DESCRIBE MATCHES "^v?[0-9]+\\.[0-9]+\\.[0-9]+")
    set(ABCTRAIN_VERSION_STRING ${ABCTRAIN_GIT_DESCRIBE})
else()
    set(ABCTRAIN_VERSION_STRING "0.0.0-dev+sha${ABCTRAIN_GIT_DESCRIBE}")
endif()

message(STATUS "abcTrain version: ${ABCTRAIN_VERSION_STRING} (git describe: ${ABCTRAIN_GIT_DESCRIBE})")

# The installers and the release workflow need the same version the app
# reports, not the project() literal - which is exactly the hand-bumped
# number ADR 012 moved away from. Writing it to a file keeps `git
# describe` the single source of truth for all of them.
#
# Filename-safe: strip a leading "v" and everything from the first
# pre-release/build separator, so "v1.0.0-3-gabc123" becomes "1.0.0".
string(REGEX REPLACE "^v" "" ABCTRAIN_VERSION_PLAIN "${ABCTRAIN_VERSION_STRING}")
string(REGEX REPLACE "[-+].*$" "" ABCTRAIN_VERSION_PLAIN "${ABCTRAIN_VERSION_PLAIN}")
if(NOT ABCTRAIN_VERSION_PLAIN MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    set(ABCTRAIN_VERSION_PLAIN "${PROJECT_VERSION}")
endif()
file(WRITE "${CMAKE_BINARY_DIR}/abctrain-version.txt" "${ABCTRAIN_VERSION_PLAIN}")
message(STATUS "abcTrain packaging version: ${ABCTRAIN_VERSION_PLAIN}")

configure_file(
    shared/VersionInfo.h.in
    ${CMAKE_BINARY_DIR}/generated/shared/VersionInfo.h
    @ONLY
)

# Embeds shared/i18n/strings/*.json into the binary as BinaryData (rather
# than reading JSON files from a filesystem path relative to the plugin
# binary, which isn't reliable across VST3/AU/Standalone install
# locations on every OS) - see shared/i18n/LocalisationManager.cpp for
# how each language code maps to its BinaryData symbol.
juce_add_binary_data(I18nData SOURCES
    shared/i18n/strings/en.json
    shared/i18n/strings/ru.json
    shared/i18n/strings/de.json
    shared/i18n/strings/fr.json
    shared/i18n/strings/es.json
    shared/i18n/strings/pt.json
    shared/i18n/strings/zh-Hans.json
    shared/i18n/strings/ja.json
    shared/i18n/strings/ko.json
    shared/i18n/strings/it.json
    shared/i18n/strings/pl.json
    shared/i18n/strings/uk.json
)

# Embeds a handful of short, originally-synthesized (not recorded/sampled
# from any third-party source) WAV files so EarTrainer's opt-in "Built-in
# Samples" training category (see Source/ReferenceAudioLibrary.cpp) works
# out of the box, with no external folder required - same "can't rely on
# a filesystem path relative to the plugin binary" reasoning as I18nData
# above. See decisions/018 for why these are programmatically generated
# rather than real recordings. NAMESPACE/HEADER_NAME are explicit and
# distinct from I18nData's (both default to BinaryData/BinaryData.h) -
# EarTrainer and EarTrainerTests link both binary-data targets at once,
# and two same-named generated headers on one include path would collide.
juce_add_binary_data(SampleData
    NAMESPACE SampleBinaryData
    HEADER_NAME SampleBinaryData.h
    SOURCES
        assets/samples/Percussive/Kick.wav
        assets/samples/Percussive/Snare.wav
        assets/samples/Sustained/Pad.wav
        assets/samples/Sustained/Pluck.wav
        assets/samples/Sustained/Tone.wav
)

juce_add_plugin(EarTrainer
    COMPANY_NAME "EarSnap"
    PLUGIN_MANUFACTURER_CODE Esnp
    PLUGIN_CODE Eqg1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Ear Trainer"
    ICON_BIG assets/icons/eartrainer.png
    ICON_SMALL assets/icons/eartrainer_small.png
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Tools
    AU_MAIN_TYPE kAudioUnitType_Effect
    NEEDS_CURL TRUE
)

target_sources(EarTrainer
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ChoiceSliderComponent.cpp
        Source/GameManager.cpp
        Source/ProgressManager.cpp
        Source/Achievements.cpp
        Source/SessionManager.cpp
        Source/ReferenceAudioLibrary.cpp
        Source/TrainingSoundsComponent.cpp
        Source/HomeScreenComponent.cpp
        Source/SupportScreenComponent.cpp
        shared/Vectorscope.cpp
        shared/SpectrumAnalyzer.cpp
        Source/Games/EQGame.cpp
        Source/Games/CompressionGame.cpp
        Source/Games/ReverbGame.cpp
        Source/Games/PanGame.cpp
        Source/Games/DelayGame.cpp
        Source/Games/DistortionGame.cpp
        Source/Games/StereoWidthGame.cpp
        Source/Games/DBGame.cpp
        Source/Games/FrequencyRangeGame.cpp
        shared/UpdateChecker.cpp
        shared/AbcTrainLookAndFeel.cpp
        shared/AbcTrainTheme.cpp
        shared/WidgetStateRegistry.cpp
        shared/CompactSelector.cpp
        shared/AppIcons.cpp
        shared/i18n/LocalisationManager.cpp
)

target_compile_definitions(EarTrainer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EarTrainer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_animation
        I18nData
        SampleData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LearnerEQ
    COMPANY_NAME "EarSnap"
    PLUGIN_MANUFACTURER_CODE Esnp
    PLUGIN_CODE Leq1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Learner EQ"
    ICON_BIG assets/icons/learnereq.png
    ICON_SMALL assets/icons/learnereq_small.png
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    NEEDS_CURL TRUE
)

target_sources(LearnerEQ
    PRIVATE
        LearnerEQ/Source/PluginProcessor.cpp
        LearnerEQ/Source/PluginEditor.cpp
        LearnerEQ/Source/SpectrumAnalyser.cpp
        LearnerEQ/Source/PluginEntry.cpp
        shared/SpectrumAnalyzer.cpp
        shared/WaveformDisplay.cpp
        shared/GainReductionMeter.cpp
        shared/GuideTooltip.cpp
        shared/LessonController.cpp
        shared/UpdateChecker.cpp
        shared/AbcTrainLookAndFeel.cpp
        shared/AbcTrainTheme.cpp
        shared/WidgetStateRegistry.cpp
        shared/CompactSelector.cpp
        shared/AppIcons.cpp
        shared/i18n/LocalisationManager.cpp
)

target_compile_definitions(LearnerEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LearnerEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        I18nData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LearnerComp
    COMPANY_NAME "EarSnap"
    PLUGIN_MANUFACTURER_CODE Esnp
    PLUGIN_CODE Lcp1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Learner Comp"
    ICON_BIG assets/icons/learnercomp.png
    ICON_SMALL assets/icons/learnercomp_small.png
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    NEEDS_CURL TRUE
)

target_sources(LearnerComp
    PRIVATE
        LearnerComp/Source/PluginProcessor.cpp
        LearnerComp/Source/PluginEditor.cpp
        shared/WaveformDisplay.cpp
        shared/GainReductionMeter.cpp
        shared/GuideTooltip.cpp
        shared/SpectrumAnalyzer.cpp
        shared/LessonController.cpp
        shared/UpdateChecker.cpp
        shared/AbcTrainLookAndFeel.cpp
        shared/AbcTrainTheme.cpp
        shared/WidgetStateRegistry.cpp
        shared/CompactSelector.cpp
        shared/AppIcons.cpp
        shared/i18n/LocalisationManager.cpp
        LearnerComp/Source/PluginEntry.cpp
)

target_compile_definitions(LearnerComp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LearnerComp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        I18nData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LearnerVerb
    COMPANY_NAME "EarSnap"
    PLUGIN_MANUFACTURER_CODE Esnp
    PLUGIN_CODE Lvb1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Learner Verb"
    ICON_BIG assets/icons/learnerverb.png
    ICON_SMALL assets/icons/learnerverb_small.png
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect
    NEEDS_CURL TRUE
)

target_sources(LearnerVerb
    PRIVATE
        LearnerVerb/Source/PluginProcessor.cpp
        LearnerVerb/Source/PluginEditor.cpp
        shared/WaveformDisplay.cpp
        shared/GainReductionMeter.cpp
        shared/GuideTooltip.cpp
        shared/SpectrumAnalyzer.cpp
        shared/LessonController.cpp
        shared/UpdateChecker.cpp
        shared/AbcTrainLookAndFeel.cpp
        shared/AbcTrainTheme.cpp
        shared/WidgetStateRegistry.cpp
        shared/CompactSelector.cpp
        shared/AppIcons.cpp
        shared/i18n/LocalisationManager.cpp
        LearnerVerb/Source/PluginEntry.cpp
)

target_compile_definitions(LearnerVerb
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LearnerVerb
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        I18nData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Unit tests. Compiles the game/processor sources directly (not the plugin
# targets themselves) so tests run as a plain console app - no plugin host
# or GUI needed. Excludes EarTrainer's Source/PluginProcessor.cpp entirely
# (the game logic under test doesn't need the AudioProcessor wrapper), and
# excludes every plugin's PluginEntry.cpp specifically: LearnerEQ,
# LearnerComp, LearnerVerb, and (if it existed) EarTrainer's own
# createPluginFilter() would all collide if more than one were linked into
# this one binary.
juce_add_console_app(EarTrainerTests
    PRODUCT_NAME "EarTrainer Tests"
)

target_sources(EarTrainerTests
    PRIVATE
        tests/TestRunner.cpp
        tests/EQGameTest.cpp
        tests/CompressionGameTest.cpp
        tests/ReverbGameTest.cpp
        tests/PanGameTest.cpp
        tests/DelayGameTest.cpp
        tests/DistortionGameTest.cpp
        tests/StereoWidthGameTest.cpp
        tests/DBGameTest.cpp
        tests/FrequencyRangeGameTest.cpp
        tests/GameManagerTest.cpp
        tests/LocalisationManagerTest.cpp
        tests/VersionChannelTest.cpp
        tests/ProgressManagerTest.cpp
        tests/SessionManagerTest.cpp
        tests/AchievementsTest.cpp
        tests/ContinuousScaleTest.cpp
        tests/LearnerEQTest.cpp
        tests/LearnerCompTest.cpp
        tests/LearnerVerbTest.cpp
        tests/MicroLessonTest.cpp
        tests/UpdateCheckerTest.cpp
        tests/ReferenceAudioLibraryTest.cpp
        Source/GameManager.cpp
        Source/ProgressManager.cpp
        Source/Achievements.cpp
        Source/SessionManager.cpp
        Source/ReferenceAudioLibrary.cpp
        Source/Games/EQGame.cpp
        Source/Games/CompressionGame.cpp
        Source/Games/ReverbGame.cpp
        Source/Games/PanGame.cpp
        Source/Games/DelayGame.cpp
        Source/Games/DistortionGame.cpp
        Source/Games/StereoWidthGame.cpp
        Source/Games/DBGame.cpp
        Source/Games/FrequencyRangeGame.cpp
        LearnerEQ/Source/PluginProcessor.cpp
        LearnerEQ/Source/PluginEditor.cpp
        LearnerEQ/Source/SpectrumAnalyser.cpp
        LearnerComp/Source/PluginProcessor.cpp
        LearnerComp/Source/PluginEditor.cpp
        LearnerVerb/Source/PluginProcessor.cpp
        LearnerVerb/Source/PluginEditor.cpp
        shared/WaveformDisplay.cpp
        shared/GainReductionMeter.cpp
        shared/GuideTooltip.cpp
        shared/SpectrumAnalyzer.cpp
        shared/LessonController.cpp
        shared/UpdateChecker.cpp
        shared/AbcTrainLookAndFeel.cpp
        shared/AbcTrainTheme.cpp
        shared/WidgetStateRegistry.cpp
        shared/CompactSelector.cpp
        shared/AppIcons.cpp
        shared/i18n/LocalisationManager.cpp
)

# JUCE_USE_CURL is deliberately left at its default (0) here: this test
# binary only exercises UpdateChecker's pure isNewerVersion()/
# parseReleaseJson() functions (see tests/UpdateCheckerTest.cpp), never
# checkForUpdatesAsync()'s real network call, so it doesn't need libcurl -
# unlike the four real plugin targets above, which pass NEEDS_CURL TRUE to
# juce_add_plugin() for that reason (see decisions/007-update-checker.md).
target_compile_definitions(EarTrainerTests
    PUBLIC
        JUCE_WEB_BROWSER=0
)

target_link_libraries(EarTrainerTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        I18nData
        SampleData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# shared/Version.h #includes the generated shared/VersionInfo.h (see the
# configure_file() call above) via a path that only resolves through this
# include directory, not a relative one - every target that pulls in
# shared/Version.h (all four plugin editors, hence all four plugins plus
# this test binary) needs it.

# ---------------------------------------------------------------------------
# EditorSnapshots - renders every Learner editor to a PNG, in both themes,
# with no plugin host. See tools/EditorSnapshots.cpp for why: the whole
# class of bug this project keeps shipping is invisible to the unit tests
# and obvious in a picture.
juce_add_console_app(EditorSnapshots
    PRODUCT_NAME "EditorSnapshots"
)

juce_generate_juce_header(EditorSnapshots)

target_sources(EditorSnapshots
    PRIVATE
        tools/EditorSnapshots.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/GameManager.cpp
        Source/ProgressManager.cpp
        Source/Achievements.cpp
        Source/SessionManager.cpp
        Source/ReferenceAudioLibrary.cpp
        Source/ChoiceSliderComponent.cpp
        Source/HomeScreenComponent.cpp
        Source/SupportScreenComponent.cpp
        Source/TrainingSoundsComponent.cpp
        Source/Games/EQGame.cpp
        Source/Games/CompressionGame.cpp
        Source/Games/ReverbGame.cpp
        Source/Games/PanGame.cpp
        Source/Games/DelayGame.cpp
        Source/Games/DistortionGame.cpp
        Source/Games/StereoWidthGame.cpp
        Source/Games/DBGame.cpp
        Source/Games/FrequencyRangeGame.cpp
        shared/Vectorscope.cpp
        LearnerEQ/Source/PluginProcessor.cpp
        LearnerEQ/Source/PluginEditor.cpp
        LearnerEQ/Source/SpectrumAnalyser.cpp
        LearnerComp/Source/PluginProcessor.cpp
        LearnerComp/Source/PluginEditor.cpp
        LearnerVerb/Source/PluginProcessor.cpp
        LearnerVerb/Source/PluginEditor.cpp
        shared/WaveformDisplay.cpp
        shared/GainReductionMeter.cpp
        shared/GuideTooltip.cpp
        shared/SpectrumAnalyzer.cpp
        shared/LessonController.cpp
        shared/UpdateChecker.cpp
        shared/AbcTrainLookAndFeel.cpp
        shared/AbcTrainTheme.cpp
        shared/WidgetStateRegistry.cpp
        shared/CompactSelector.cpp
        shared/AppIcons.cpp
        shared/i18n/LocalisationManager.cpp
)

target_compile_definitions(EditorSnapshots
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(EditorSnapshots
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_animation
        I18nData
        SampleData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

foreach(abcTrainTarget EarTrainer LearnerEQ LearnerComp LearnerVerb EarTrainerTests EditorSnapshots)
    target_include_directories(${abcTrainTarget} PRIVATE ${CMAKE_BINARY_DIR}/generated)
endforeach()

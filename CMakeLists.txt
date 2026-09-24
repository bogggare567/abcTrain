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

project(EarTrainer VERSION 1.1.0)

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
# Samples" training category (see) works
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

# The interface typeface, embedded because a plugin cannot ask its host's
# machine to have a font installed. Twelve faces, 356 KB: Barlow carries
# the Latin at three widths, and a Cyrillic companion pinned out of
# Roboto's variable font carries the Cyrillic at the same three widths -
# Barlow has no Cyrillic at all, so without this every Russian word in a
# Russian-first app would fall through to whatever the operating system
# happened to pick. Each file is subset to the one script it is there
# for, which is what keeps twelve faces under half a megabyte. See
# assets/fonts/README.md and tools/build_fonts.py.
juce_add_binary_data(FontData
    NAMESPACE FontBinaryData
    HEADER_NAME FontBinaryData.h
    SOURCES
        assets/fonts/Barlow-Regular.ttf
        assets/fonts/Barlow-Medium.ttf
        assets/fonts/BarlowSemiCondensed-Regular.ttf
        assets/fonts/BarlowSemiCondensed-Medium.ttf
        assets/fonts/BarlowCondensed-SemiBold.ttf
        assets/fonts/BarlowCondensed-Bold.ttf
        assets/fonts/abcTrainCyr-Regular.ttf
        assets/fonts/abcTrainCyr-Medium.ttf
        assets/fonts/abcTrainCyrSemiCondensed-Regular.ttf
        assets/fonts/abcTrainCyrSemiCondensed-Medium.ttf
        assets/fonts/abcTrainCyrCondensed-SemiBold.ttf
        assets/fonts/abcTrainCyrCondensed-Bold.ttf
)

# The app icon, embedded so the first screen can draw the *same* mark the
# player sees in their dock and Finder. It was already wired to
# juce_add_plugin's ICON_BIG, but that only reaches the platform's own
# icon slot - nothing inside the editor could read it, so the welcome
# screen was drawing an unrelated line glyph.
juce_add_binary_data(BrandData
    NAMESPACE BrandBinaryData
    HEADER_NAME BrandBinaryData.h
    SOURCES
        assets/icons/eartrainer.png
        # The licence, embedded so the About page can show it in full. A
        # licence you have to leave the app to read is a licence nobody
        # reads - and it is the one document a user is entitled to see
        # without going looking.
        LICENSE
)

# Every product name begins with "ABC", and that is a distribution
# decision rather than a stylistic one: plugin browsers sort
# alphabetically, so a suite whose four members scatter across E and L is
# four things a person has to remember to look for. Under ABC they arrive
# together, next to the pack they belong to.
#
# The vendor changed with them. "EarSnap" was a name from before this was
# abcTrain and it is what DAWs print in the manufacturer column, so
# leaving it would have made every plugin list read "ABC Learner EQ" by
# someone unrelated.
#
# **This breaks existing sessions.** JUCE derives the VST3 class ID and the
# AU subtype from the company and product names, so to a host these are new
# plugins and a project saved against the old ones will report them
# missing. There is no migration path for that - a rename is a rename. Done
# now, at two stars and one tagged release, because the cost only grows.

# ---------------------------------------------------------------------------
# shared/ as libraries (ADR 039).
#
# Every product used to list the shared .cpp files it compiled, by hand -
# seven copies of the same twenty-odd lines (four plugins, the tests, the
# editor tools, RoundBench), and a file added to one list and not another
# was a link error at best and a silently stale tool at worst. Now each
# folder of shared/ is one INTERFACE library that lists its sources once,
# and a product links the groups it uses. Linking a group pulls in the
# groups it depends on.
#
# They are INTERFACE libraries, not static ones, on purpose: JUCE code has to
# be compiled with each product's own configuration (plugin name, codes,
# JucePlugin_* defines), so the sources are compiled into every consumer,
# exactly as before - one list, same build. Link them PRIVATE: a PUBLIC link
# would hand the sources on to the VST3/AU/Standalone wrappers as well and
# define every symbol twice.
#
# Includes are written from the repository root - "shared/ui/AbcTrainTheme.h"
# - so a file can move without its includers counting ../ segments.
#
#   ui        theme, look-and-feel, fonts, icons, the small widgets, window fit
#   analysis  what the screen draws from audio: spectrum, waveform, meters
#   audio     sound that is not a plugin's processing: the training library,
#             the slicer, stems, generators
#   learning  modules, lessons and the Learner editor they live in
#   updates   version, update check, the installed-plugins list
#   i18n      the localisation manager (string tables are I18nData)
# ---------------------------------------------------------------------------
set(abcShared ${CMAKE_CURRENT_SOURCE_DIR}/shared)

add_library(abc_i18n INTERFACE)
target_sources(abc_i18n INTERFACE ${abcShared}/i18n/LocalisationManager.cpp)
target_include_directories(abc_i18n INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(abc_ui INTERFACE)
target_sources(abc_ui INTERFACE
    ${abcShared}/ui/AbcTrainTheme.cpp
    ${abcShared}/ui/AbcTrainLookAndFeel.cpp
    ${abcShared}/ui/AbcTrainFonts.cpp
    ${abcShared}/ui/AppIcons.cpp
    ${abcShared}/ui/CompactSelector.cpp
    ${abcShared}/ui/WidgetStateRegistry.cpp
    ${abcShared}/ui/GuideTooltip.cpp
    ${abcShared}/ui/TourOverlay.cpp
    ${abcShared}/ui/IdleScreensaver.cpp
)
target_link_libraries(abc_ui INTERFACE abc_i18n)

add_library(abc_analysis INTERFACE)
target_sources(abc_analysis INTERFACE
    ${abcShared}/analysis/SpectrumAnalyzer.cpp
    ${abcShared}/analysis/WaveformDisplay.cpp
    ${abcShared}/analysis/GainReductionMeter.cpp
    ${abcShared}/analysis/Vectorscope.cpp
)
target_link_libraries(abc_analysis INTERFACE abc_ui)

add_library(abc_audio INTERFACE)
target_sources(abc_audio INTERFACE
    ${abcShared}/audio/ReferenceAudioLibrary.cpp
    ${abcShared}/audio/AudioSliceAnalyzer.cpp
    ${abcShared}/audio/LessonAudioBed.cpp
)
target_include_directories(abc_audio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# The effect engines (ADR 040): one reverb, one compressor, one set of EQ
# coefficients, shared by the trainer's exercises and the Learner plugins,
# so what a player learns to hear is what the plugin they then turn does.
# Header-only; depends on nothing but JUCE.
add_library(abc_dsp INTERFACE)
target_include_directories(abc_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(abc_updates INTERFACE)
target_sources(abc_updates INTERFACE
    ${abcShared}/updates/UpdateChecker.cpp
    ${abcShared}/updates/UpdatePrompt.cpp
    ${abcShared}/updates/UpdateWindow.cpp
)
target_link_libraries(abc_updates INTERFACE abc_ui)

add_library(abc_learning INTERFACE)
target_sources(abc_learning INTERFACE
    ${abcShared}/learning/TrainingModule.cpp
    ${abcShared}/learning/ModuleProgress.cpp
    ${abcShared}/learning/ModuleScreenComponent.cpp
    ${abcShared}/learning/LearnerEditorBase.cpp
    ${abcShared}/learning/CompanionPanel.cpp
    ${abcShared}/learning/PracticeSourceSelector.cpp
)
target_link_libraries(abc_learning INTERFACE abc_ui abc_analysis abc_audio abc_dsp abc_updates)

# Everything, for the test binary and the editor tools, which exercise all
# four products at once.
add_library(abc_shared INTERFACE)
target_link_libraries(abc_shared INTERFACE abc_ui abc_analysis abc_audio abc_dsp abc_updates abc_learning)

# The trainer's engine (ADR 039): the nine exercises and the rules around
# them - which round comes next, the staircase, what is saved, achievements,
# the hearing dose. None of it draws anything, and that is the seam the
# Teaching and Live layers will need: an exercise that can be asked and
# graded without the trainer's window. The trainer, the tests and the
# editor tools link it instead of listing the same fourteen files three
# times. (RoundBench keeps its own short list: it times six games and must
# not pull in the managers.)
add_library(abc_trainer_engine INTERFACE)
target_sources(abc_trainer_engine INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/GameManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/SessionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/ProgressManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Achievements.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/HearingGuard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Games/EQGame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Games/CompressionGame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Games/ReverbGame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Games/PanGame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Games/DelayGame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Games/DistortionGame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Games/StereoWidthGame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Games/DBGame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Games/FrequencyRangeGame.cpp
)
target_link_libraries(abc_trainer_engine INTERFACE abc_audio abc_dsp abc_i18n)

# The three Learner processors and editors, each as a library (ADR 041).
# The plugins link one each and add their own PluginEntry.cpp - the only
# file that defines createPluginFilter(), so it must never be in here. The
# abcTrain app links all three and runs them inside its Studio tab; the
# tests and the editor tools link them for the same reason.
add_library(abc_learner_eq INTERFACE)
target_sources(abc_learner_eq INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/LearnerEQ/Source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LearnerEQ/Source/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LearnerEQ/Source/SpectrumAnalyser.cpp
)
target_link_libraries(abc_learner_eq INTERFACE abc_learning)

add_library(abc_learner_comp INTERFACE)
target_sources(abc_learner_comp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/LearnerComp/Source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LearnerComp/Source/PluginEditor.cpp
)
target_link_libraries(abc_learner_comp INTERFACE abc_learning)

add_library(abc_learner_verb INTERFACE)
target_sources(abc_learner_verb INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/LearnerVerb/Source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LearnerVerb/Source/PluginEditor.cpp
)
target_link_libraries(abc_learner_verb INTERFACE abc_learning)

add_library(abc_learners INTERFACE)
target_link_libraries(abc_learners INTERFACE abc_learner_eq abc_learner_comp abc_learner_verb)

juce_add_plugin(EarTrainer
    COMPANY_NAME "soundkorb"
    PLUGIN_MANUFACTURER_CODE Sndk
    PLUGIN_CODE Eqg1
    FORMATS Standalone
    PRODUCT_NAME "ABC Ear Trainer"
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
        Source/TrainingSoundsComponent.cpp
        Source/HomeScreenComponent.cpp
        Source/SupportScreenComponent.cpp
        Source/SettingsScreenComponent.cpp
        Source/RunResultsComponent.cpp
        Source/TopNavComponent.cpp
        Source/AchievementsScreenComponent.cpp
        Source/StudioScreenComponent.cpp
        Source/LiveScreenComponent.cpp
)

target_compile_definitions(EarTrainer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EarTrainer
    PRIVATE
        abc_trainer_engine
        abc_learners
        abc_ui
        abc_analysis
        abc_audio
        abc_updates
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_animation
        I18nData
        FontData
        SampleData
        BrandData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LearnerEQ
    COMPANY_NAME "soundkorb"
    PLUGIN_MANUFACTURER_CODE Sndk
    PLUGIN_CODE Leq1
    FORMATS AU VST3
    PRODUCT_NAME "ABC Learner EQ"
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
        LearnerEQ/Source/PluginEntry.cpp
)

target_compile_definitions(LearnerEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LearnerEQ
    PRIVATE
        abc_learner_eq
        juce::juce_audio_utils
        juce::juce_dsp
        I18nData
        FontData
        SampleData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LearnerComp
    COMPANY_NAME "soundkorb"
    PLUGIN_MANUFACTURER_CODE Sndk
    PLUGIN_CODE Lcp1
    FORMATS AU VST3
    PRODUCT_NAME "ABC Learner Comp"
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
        LearnerComp/Source/PluginEntry.cpp
)

target_compile_definitions(LearnerComp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LearnerComp
    PRIVATE
        abc_learner_comp
        juce::juce_audio_utils
        juce::juce_dsp
        I18nData
        FontData
        SampleData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LearnerVerb
    COMPANY_NAME "soundkorb"
    PLUGIN_MANUFACTURER_CODE Sndk
    PLUGIN_CODE Lvb1
    FORMATS AU VST3
    PRODUCT_NAME "ABC Learner Verb"
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
        LearnerVerb/Source/PluginEntry.cpp
)

target_compile_definitions(LearnerVerb
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LearnerVerb
    PRIVATE
        abc_learner_verb
        juce::juce_audio_utils
        juce::juce_dsp
        I18nData
        FontData
        SampleData
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
        tests/GainMatchTest.cpp
        tests/NoiseColourTest.cpp
        tests/StereoWidthGameTest.cpp
        tests/DBGameTest.cpp
        tests/FrequencyRangeGameTest.cpp
        tests/GameManagerTest.cpp
        tests/LocalisationManagerTest.cpp
        tests/VersionChannelTest.cpp
        tests/ProgressManagerTest.cpp
        tests/SessionManagerTest.cpp
        tests/AchievementsTest.cpp
        tests/AudioSliceAnalyzerTest.cpp
    tests/RealtimeSafetyTest.cpp
        tests/PracticeAudioSourceTest.cpp
        tests/TrainingModuleTest.cpp
        tests/LessonAudioBedTest.cpp
        tests/ContinuousScaleTest.cpp
        tests/LearnerEQTest.cpp
        tests/LearnerCompTest.cpp
        tests/LearnerVerbTest.cpp
        tests/MicroLessonTest.cpp
        tests/UpdateCheckerTest.cpp
        tests/ReferenceAudioLibraryTest.cpp
        tests/TestSignalGeneratorTest.cpp
        tests/SharedEnginesTest.cpp
        tests/HintTest.cpp
        tests/SkillBucketTest.cpp
        tests/HearingGuardTest.cpp
        tests/LearnerRedesignTest.cpp
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
        abc_trainer_engine
        abc_shared
        abc_learners
        juce::juce_audio_utils
        juce::juce_dsp
        I18nData
        FontData
        SampleData
        BrandData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# shared/updates/Version.h #includes the generated shared/VersionInfo.h (see the
# configure_file() call above) via a path that only resolves through this
# include directory, not a relative one - every target that pulls in
# shared/updates/Version.h (all four plugin editors, hence all four plugins plus
# this test binary) needs it.

# ---------------------------------------------------------------------------
# EditorSnapshots - renders every Learner editor to a PNG, in both themes,
# with no plugin host. See tools/EditorSnapshots.cpp for why: the whole
# class of bug this project keeps shipping is invisible to the unit tests
# and obvious in a picture.
# The editor tools - the snapshot renderer and the click map - are the
# same program with a different question, so they compile the same
# fifty files. One list, because two would drift the first time a
# screen was added to only one of them.
set(abcTrainEditorToolSources
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ChoiceSliderComponent.cpp
        Source/HomeScreenComponent.cpp
        Source/SupportScreenComponent.cpp
        Source/SettingsScreenComponent.cpp
        Source/RunResultsComponent.cpp
        Source/TopNavComponent.cpp
        Source/AchievementsScreenComponent.cpp
        Source/TrainingSoundsComponent.cpp
        Source/StudioScreenComponent.cpp
        Source/LiveScreenComponent.cpp
)

juce_add_console_app(EditorSnapshots
    PRODUCT_NAME "EditorSnapshots"
)

juce_generate_juce_header(EditorSnapshots)

target_sources(EditorSnapshots
    PRIVATE
        tools/EditorSnapshots.cpp
        ${abcTrainEditorToolSources}
)

target_compile_definitions(EditorSnapshots
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(EditorSnapshots
    PRIVATE
        abc_trainer_engine
        abc_shared
        abc_learners
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_animation
        I18nData
        FontData
        SampleData
        BrandData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# What can actually be pressed. EditorSnapshots answers "what does this
# look like"; a control can look perfectly present in a picture and still
# be unreachable because something transparent lies over it, or it was
# parked at empty bounds. Both read to a player as a broken app, and
# neither is visible in a snapshot - see tools/ClickMap.cpp.
# Prints every English string the Learner plugins show, as i18n keys - the
# starting point for translating them (see tools/LearnerStrings.cpp).
juce_add_console_app(LearnerStrings
    PRODUCT_NAME "LearnerStrings"
)

juce_generate_juce_header(LearnerStrings)

target_sources(LearnerStrings
    PRIVATE
        tools/LearnerStrings.cpp
        ${abcTrainEditorToolSources}
)

target_compile_definitions(LearnerStrings
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(LearnerStrings
    PRIVATE
        abc_trainer_engine
        abc_shared
        abc_learners
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_animation
        I18nData
        FontData
        SampleData
        BrandData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

juce_add_console_app(ClickMap
    PRODUCT_NAME "ClickMap"
)

juce_generate_juce_header(ClickMap)

target_sources(ClickMap
    PRIVATE
        tools/ClickMap.cpp
        ${abcTrainEditorToolSources}
)

target_compile_definitions(ClickMap
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(ClickMap
    PRIVATE
        abc_trainer_engine
        abc_shared
        abc_learners
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_animation
        I18nData
        FontData
        SampleData
        BrandData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

foreach(abcTrainTarget EarTrainer LearnerEQ LearnerComp LearnerVerb EarTrainerTests EditorSnapshots ClickMap LearnerStrings)
    target_include_directories(${abcTrainTarget} PRIVATE ${CMAKE_BINARY_DIR}/generated)
endforeach()

# Temporary: measures what a round transition costs on the message thread.
juce_add_console_app(RoundBench PRODUCT_NAME "RoundBench")
target_sources(RoundBench PRIVATE
    tools/RoundBench.cpp
    Source/Games/ReverbGame.cpp Source/Games/DelayGame.cpp
    Source/Games/EQGame.cpp Source/Games/CompressionGame.cpp
    Source/Games/DistortionGame.cpp Source/Games/FrequencyRangeGame.cpp
)
target_compile_definitions(RoundBench PRIVATE JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(RoundBench PRIVATE abc_audio abc_dsp juce::juce_dsp juce::juce_audio_formats
    juce::juce_audio_basics juce::juce_core juce::juce_events
    juce::juce_data_structures SampleData
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

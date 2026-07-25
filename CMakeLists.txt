cmake_minimum_required(VERSION 3.22)

project(EarTrainer VERSION 0.1.0)

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

juce_add_plugin(EarTrainer
    COMPANY_NAME "EarSnap"
    PLUGIN_MANUFACTURER_CODE Esnp
    PLUGIN_CODE Eqg1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Ear Trainer"
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
        Source/GameManager.cpp
        Source/ProgressManager.cpp
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
        I18nData
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
        shared/LessonController.cpp
        shared/UpdateChecker.cpp
        shared/AbcTrainLookAndFeel.cpp
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
        shared/SpectrumAnalyzer.cpp
        shared/LessonController.cpp
        shared/UpdateChecker.cpp
        shared/AbcTrainLookAndFeel.cpp
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
        shared/SpectrumAnalyzer.cpp
        shared/LessonController.cpp
        shared/UpdateChecker.cpp
        shared/AbcTrainLookAndFeel.cpp
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
        tests/ProgressManagerTest.cpp
        tests/LearnerEQTest.cpp
        tests/LearnerCompTest.cpp
        tests/LearnerVerbTest.cpp
        tests/MicroLessonTest.cpp
        tests/UpdateCheckerTest.cpp
        Source/GameManager.cpp
        Source/ProgressManager.cpp
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
        shared/SpectrumAnalyzer.cpp
        shared/LessonController.cpp
        shared/UpdateChecker.cpp
        shared/AbcTrainLookAndFeel.cpp
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
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

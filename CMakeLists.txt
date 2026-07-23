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
)

target_sources(EarTrainer
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/GameManager.cpp
        Source/Games/EQGame.cpp
        Source/Games/CompressionGame.cpp
)

target_compile_definitions(EarTrainer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EarTrainer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
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
)

target_sources(LearnerEQ
    PRIVATE
        LearnerEQ/Source/PluginProcessor.cpp
        LearnerEQ/Source/PluginEditor.cpp
        LearnerEQ/Source/SpectrumAnalyser.cpp
)

target_compile_definitions(LearnerEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LearnerEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Unit tests. Compiles the game/processor sources directly (not the plugin
# targets themselves) so tests run as a plain console app - no plugin host
# or GUI needed. Excludes EarTrainer's Source/PluginProcessor.cpp, since
# both it and LearnerEQ's PluginProcessor.cpp define createPluginFilter();
# linking both into one binary would collide, and the game logic under
# test doesn't need the AudioProcessor wrapper anyway.
juce_add_console_app(EarTrainerTests
    PRODUCT_NAME "EarTrainer Tests"
)

target_sources(EarTrainerTests
    PRIVATE
        tests/TestRunner.cpp
        tests/EQGameTest.cpp
        tests/CompressionGameTest.cpp
        tests/GameManagerTest.cpp
        tests/LearnerEQTest.cpp
        Source/GameManager.cpp
        Source/Games/EQGame.cpp
        Source/Games/CompressionGame.cpp
        LearnerEQ/Source/PluginProcessor.cpp
        LearnerEQ/Source/PluginEditor.cpp
        LearnerEQ/Source/SpectrumAnalyser.cpp
)

target_compile_definitions(EarTrainerTests
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(EarTrainerTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

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

cmake_minimum_required(VERSION 3.22)

project(PrimaTakePerspective VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.2
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PrimaTakePerspective
    COMPANY_NAME "Prima Take"
    BUNDLE_ID "it.primatake.perspective"
    PLUGIN_MANUFACTURER_CODE PrTk
    PLUGIN_CODE PtPs

    FORMATS
        VST3
        AU
        Standalone

    PRODUCT_NAME "Prima Take Perspective"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(PrimaTakePerspective
    PRIVATE
        PluginProcessor.cpp
        PluginProcessor.h
        PluginEditor.cpp
        PluginEditor.h
)

target_compile_definitions(PrimaTakePerspective
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(PrimaTakePerspective
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

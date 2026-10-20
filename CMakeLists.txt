cmake_minimum_required(VERSION 3.22)
project(PitchlyGraph VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static MSVC runtime: no VC++ redistributable needed (handy on fresh ARM64 installs)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(PITCHLYGRAPH_JUCE_DIR "" CACHE PATH "Optional: path to an existing JUCE checkout (otherwise it is downloaded)")
if (PITCHLYGRAPH_JUCE_DIR)
    add_subdirectory(${PITCHLYGRAPH_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(PitchlyGraph
    COMPANY_NAME "Ethan"
    PLUGIN_MANUFACTURER_CODE Ethn
    PLUGIN_CODE Pgrp
    FORMATS VST3
    PRODUCT_NAME "PitchlyGraph"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx "Pitch Shift"
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(PitchlyGraph PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h
    Source/PianoRoll.cpp Source/PianoRoll.h
    Source/Dsp.cpp Source/Dsp.h
    Source/UI.cpp Source/UI.h)

target_compile_definitions(PitchlyGraph PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PitchlyGraph
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

cmake_minimum_required(VERSION 3.22)
project(VoixMidi VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VoixMidi
    COMPANY_NAME "nyxover"
    PLUGIN_MANUFACTURER_CODE Nyxo
    PLUGIN_CODE VxMd
    FORMATS VST3
    PRODUCT_NAME "Voix MIDI"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(VoixMidi PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(VoixMidi PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VoixMidi
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

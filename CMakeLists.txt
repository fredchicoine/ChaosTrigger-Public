cmake_minimum_required(VERSION 3.22)
project(ChaosTrigger VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(JUCE)

juce_add_plugin(ChaosTrigger
    COMPANY_NAME "iNi"
    PLUGIN_MANUFACTURER_CODE Inii
    PLUGIN_CODE Chtr
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Chaos Trigger"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Demo loops: ON by default when Resources/Samples exists. Public builds use -DCHAOS_DEMO_SAMPLES=OFF.
file(GLOB FACTORY_SAMPLES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Samples/*.wav")
if(FACTORY_SAMPLES)
    option(CHAOS_DEMO_SAMPLES "Build the demo loops into the plugin" ON)
else()
    set(CHAOS_DEMO_SAMPLES OFF)
endif()
if(CHAOS_DEMO_SAMPLES)
    juce_add_binary_data(ChaosTriggerData SOURCES ${FACTORY_SAMPLES})
    set_target_properties(ChaosTriggerData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    target_compile_definitions(ChaosTriggerData INTERFACE CHAOS_HAS_DEMO_SAMPLES=1)
else()
    add_library(ChaosTriggerData INTERFACE)
endif()
message(STATUS "Chaos Trigger demo samples: ${CHAOS_DEMO_SAMPLES}")

target_sources(ChaosTrigger PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ChaosTrigger PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ChaosTrigger PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    ChaosTriggerData
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Headless engine/GUI test (Linux only)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
juce_add_console_app(ChaosTriggerTest PRODUCT_NAME "ChaosTriggerTest")
target_sources(ChaosTriggerTest PRIVATE Tests/RenderTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(ChaosTriggerTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Build_Standalone=0)
target_link_libraries(ChaosTriggerTest PRIVATE ChaosTriggerData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

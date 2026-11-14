cmake_minimum_required(VERSION 3.22)
project(BTrigger VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout with -DBTRIGGER_JUCE_DIR=/path/to/JUCE, otherwise JUCE is fetched.
if(BTRIGGER_JUCE_DIR)
    add_subdirectory(${BTRIGGER_JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# AU is macOS only, so it is only added there (Logic and GarageBand load AU, not VST3).
set(BTRIGGER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND BTRIGGER_FORMATS AU)
endif()

juce_add_plugin(BTrigger
    COMPANY_NAME "Bartists Info"
    PLUGIN_MANUFACTURER_CODE Bart
    PLUGIN_CODE Btrg
    FORMATS ${BTRIGGER_FORMATS}
    PRODUCT_NAME "B Trigger"
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    BUNDLE_ID info.bartists.btrigger
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(BTrigger PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/FactorySamples.cpp)

target_compile_definitions(BTrigger PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(BTrigger
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

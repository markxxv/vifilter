cmake_minimum_required(VERSION 3.22)

project(ViFilter VERSION 0.4.0 LANGUAGES C CXX)

include(CTest)
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(juce)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

juce_add_plugin(ViFilter
    COMPANY_NAME "Vi Audio"
    BUNDLE_ID "com.markxxv.vifilter"
    PLUGIN_MANUFACTURER_CODE Vifr
    PLUGIN_CODE Vfil
    PLUGIN_NAME "Vi Filter"
    PRODUCT_NAME "Vi Filter"
    DESCRIPTION "Vi Filter audio effect"
    VERSION ${PROJECT_VERSION}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    FORMATS AU Standalone
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(ViFilter PRIVATE
    Source/AnalogDSP.h
    Source/FilterDSP.h
    Source/PresetEngine.h
    Source/PresetEngine.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
)

target_compile_definitions(ViFilter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(ViFilter PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(BUILD_TESTING)
    add_executable(ViFilterDSPTests Tests/FilterDSPTests.cpp)
    target_include_directories(ViFilterDSPTests PRIVATE Source)
    target_compile_features(ViFilterDSPTests PRIVATE cxx_std_17)
    add_test(NAME ViFilterDSPTests COMMAND ViFilterDSPTests)

    add_executable(ViFilterAnalogTests Tests/AnalogDSPTests.cpp)
    target_include_directories(ViFilterAnalogTests PRIVATE Source)
    target_compile_features(ViFilterAnalogTests PRIVATE cxx_std_17)
    add_test(NAME ViFilterAnalogTests COMMAND ViFilterAnalogTests)

    add_executable(ViFilterOversamplingTests Tests/OversamplingTests.cpp)
    target_include_directories(ViFilterOversamplingTests PRIVATE Source)
    target_link_libraries(ViFilterOversamplingTests PRIVATE
        juce::juce_dsp
        juce::juce_recommended_config_flags
    )
    target_compile_definitions(ViFilterOversamplingTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    add_test(NAME ViFilterOversamplingTests COMMAND ViFilterOversamplingTests)

    add_executable(ViFilterPresetTests
        Tests/PresetEngineTests.cpp
        Source/PresetEngine.cpp
        Source/PresetEngine.h
    )
    target_include_directories(ViFilterPresetTests PRIVATE Source)
    target_link_libraries(ViFilterPresetTests PRIVATE
        juce::juce_core
        juce::juce_recommended_config_flags
    )
    target_compile_definitions(ViFilterPresetTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    add_test(NAME ViFilterPresetTests COMMAND ViFilterPresetTests)
endif()

cmake_minimum_required(VERSION 3.22)
project(MKRattusTranceGate VERSION 1.0.0 LANGUAGES C CXX)

set(MK_JUCE_SOURCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout")
if(NOT MK_JUCE_SOURCE_DIR)
    get_filename_component(_local_juce "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" ABSOLUTE)
    if(EXISTS "${_local_juce}/CMakeLists.txt")
        set(MK_JUCE_SOURCE_DIR "${_local_juce}")
    endif()
endif()

if(MK_JUCE_SOURCE_DIR)
    add_subdirectory("${MK_JUCE_SOURCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.5
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # Standalone is what wraps the AUv3 appex into an installable app on iOS.
    set(plugin_formats AUv3 Standalone)
elseif(APPLE)
    set(plugin_formats VST3 AUv3 Standalone)
else()
    set(plugin_formats VST3 Standalone)
endif()

juce_add_plugin(MKRattusTranceGate
    COMPANY_NAME "MK"
    BUNDLE_ID "com.mk.rattustrancegate"
    PLUGIN_MANUFACTURER_CODE Mkno
    PLUGIN_CODE Rtgt
    FORMATS ${plugin_formats}
    PRODUCT_NAME "MK RATTUS TRANCE GATE"
    DESCRIPTION "Tempo-synced rhythmic gating effect with editable Gate12 curves"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationPortrait UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight)

target_sources(MKRattusTranceGate PRIVATE
    ThirdParty/Gate12/src/dsp/Pattern.cpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/GatePresets.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)
target_compile_features(MKRattusTranceGate PRIVATE cxx_std_17)
target_compile_definitions(MKRattusTranceGate PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(MKRattusTranceGate PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
target_include_directories(MKRattusTranceGate PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Gate12/src/dsp")

if(TARGET MKRattusTranceGate_VST3)
    add_custom_command(TARGET MKRattusTranceGate_VST3 POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:MKRattusTranceGate_VST3>/../Resources"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Gate12/LICENSE"
            "$<TARGET_FILE_DIR:MKRattusTranceGate_VST3>/../Resources/Gate12-LICENSE.txt")
endif()

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    juce_add_console_app(MKRattusTranceGateTests PRODUCT_NAME "MK Rattus Trance Gate Tests")
    target_sources(MKRattusTranceGateTests PRIVATE Tests/PluginTests.cpp)
    target_compile_features(MKRattusTranceGateTests PRIVATE cxx_std_17)
    target_link_libraries(MKRattusTranceGateTests PRIVATE
        MKRattusTranceGate
        juce::juce_audio_utils)
    add_test(NAME MKRattusTranceGateTests COMMAND MKRattusTranceGateTests)
    set_tests_properties(MKRattusTranceGateTests PROPERTIES TIMEOUT 30)
endif()

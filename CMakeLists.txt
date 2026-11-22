cmake_minimum_required(VERSION 3.22)

project(MKAmbient VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "macOS deployment target")
set(CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0" CACHE STRING "iOS deployment target")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(MK_BUILD_PLUGIN "Build the JUCE plugin targets (AUv3 / Standalone / VST3)" OFF)
option(MK_INSTALL_PLUGINS "Copy built plug-ins into the system plug-in folder after building" OFF)
option(MK_BUILD_TESTS "Build the portable DSP test suite" ON)
option(MK_BUILD_RENDERER "Build the offline preset renderer" ON)
set(MK_JUCE_VERSION "8.0.10" CACHE STRING "JUCE version to fetch when MK_BUILD_PLUGIN=ON")

# ---------------------------------------------------------------------------
# Portable DSP core. No external dependencies: this library carries all audio
# processing and is what the tests and the offline renderer exercise.
# ---------------------------------------------------------------------------
set(MK_DSP_SOURCES
    Source/DSP/Params.cpp
    Source/DSP/ParamStore.cpp
    Source/DSP/Filters.cpp
    Source/DSP/Wavetables.cpp
    Source/DSP/NoiseGen.cpp
    Source/DSP/SampleLibrary.cpp
    Source/DSP/SamplePlayer.cpp
    Source/DSP/Effects.cpp
    Source/DSP/Voice.cpp
    Source/DSP/NoiseEngine.cpp
    Source/DSP/SynthEngine.cpp
    Source/DSP/TranceGate.cpp
    Source/DSP/Presets.cpp
)

add_library(mkambient_core STATIC ${MK_DSP_SOURCES})
target_include_directories(mkambient_core PUBLIC Source)
target_compile_features(mkambient_core PUBLIC cxx_std_17)

if (MSVC)
    target_compile_options(mkambient_core PRIVATE /W3 /permissive- /fp:precise)
    target_compile_definitions(mkambient_core PRIVATE _USE_MATH_DEFINES NOMINMAX)
else()
    target_compile_options(mkambient_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Aggressive release optimisation for the Apple targets (iOS / iPadOS AUv3 on
# A10X-class hardware). -fno-math-errno matters most: without it every exp2f /
# tanf in the coefficient paths keeps the errno side effect and cannot become a
# plain instruction. -ffast-math is deliberately not used, because the DSP
# relies on explicit denormal flushing and on NaN-free comparisons.
if (APPLE AND NOT MSVC)
    target_compile_options(mkambient_core PRIVATE
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-fno-math-errno>
        $<$<CONFIG:Release>:-fno-trapping-math>
        $<$<CONFIG:Release>:-fomit-frame-pointer>
        $<$<CONFIG:Release>:-fvisibility=hidden>
        $<$<CONFIG:Release>:-fvisibility-inlines-hidden>
    )
endif()

# ---------------------------------------------------------------------------
# Test suite and offline renderer (host independent). Neither can link for an
# iOS device: command line tools are desktop-only, so they are skipped there.
# ---------------------------------------------------------------------------
if (CMAKE_SYSTEM_NAME STREQUAL "iOS" OR IOS)
    set(MK_TARGET_IOS TRUE)
else()
    set(MK_TARGET_IOS FALSE)
endif()

if (MK_BUILD_TESTS AND NOT MK_TARGET_IOS)
    add_executable(mk_tests
        Tests/TestMain.cpp
        Tests/TestFilters.cpp
        Tests/TestGenerators.cpp
        Tests/TestEffects.cpp
        Tests/TestGate.cpp
        Tests/TestEngine.cpp
    )
    target_link_libraries(mk_tests PRIVATE mkambient_core)
    if (MSVC)
        target_compile_options(mk_tests PRIVATE /W3 /permissive-)
    endif()
    enable_testing()
    add_test(NAME mk_tests COMMAND mk_tests)
endif()

if (MK_BUILD_RENDERER AND NOT MK_TARGET_IOS)
    add_executable(mk_render Tools/RenderAudio.cpp)
    target_link_libraries(mk_render PRIVATE mkambient_core)

    add_executable(mk_bench Tools/Benchmark.cpp)
    target_link_libraries(mk_bench PRIVATE mkambient_core)
endif()

# ---------------------------------------------------------------------------
# Plugin wrapper (JUCE). AUv3 + Standalone for iPad/iOS, plus desktop formats
# for development. Requires a host with Xcode (iOS) or MSVC/Clang (desktop).
# ---------------------------------------------------------------------------
if (MK_BUILD_PLUGIN)
    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_Declare(JUCE
        URL "https://github.com/juce-framework/JUCE/archive/refs/tags/${MK_JUCE_VERSION}.zip"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(MKAmbient
        COMPANY_NAME "MK"
        BUNDLE_ID "com.mk.ambient"
        PLUGIN_MANUFACTURER_CODE Mkau
        PLUGIN_CODE MkAb
        PRODUCT_NAME "MK Ambient"
        DESCRIPTION "Wide, evolving ambient pad and texture synthesizer"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD ${MK_INSTALL_PLUGINS}
        FORMATS Standalone VST3 AU AUv3
        MICROPHONE_PERMISSION_ENABLED FALSE
        HARDENED_RUNTIME_ENABLED TRUE
    )

    target_sources(MKAmbient PRIVATE
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp
        Source/UI/Theme.cpp
        Source/UI/Knob.cpp
        Source/UI/GateEditor.cpp
        Source/UI/Panels.cpp
    )

    target_include_directories(MKAmbient PRIVATE Source)
    # There is no VST2 build of this plug-in, so the VST2 parameter-id
    # migration guard does not apply (JUCE otherwise refuses to compile VST3).
    # MIDI CC emulation parameters are also disabled: the engine reads the
    # sustain pedal from MIDI directly, and 2000+ CC parameters only clutter
    # host automation.
    target_compile_definitions(MKAmbient PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0
    )
    target_link_libraries(MKAmbient PRIVATE
        mkambient_core
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )

    juce_generate_juce_header(MKAmbient)

    # Headless wrapper test: drives the actual AudioProcessor through
    # prepareToPlay / MIDI / processBlock and checks the audio + automation path.
    # Desktop only - a console app cannot be produced for an iOS device.
    if (NOT MK_TARGET_IOS)
        juce_add_console_app(MKPluginSmokeTest PRODUCT_NAME "MK Plugin Smoke Test")
        target_sources(MKPluginSmokeTest PRIVATE
            Tools/PluginSmokeTest.cpp
            Source/Plugin/PluginProcessor.cpp
            Source/Plugin/PluginEditor.cpp
            Source/UI/Theme.cpp
            Source/UI/Knob.cpp
            Source/UI/GateEditor.cpp
            Source/UI/Panels.cpp
        )
        target_include_directories(MKPluginSmokeTest PRIVATE Source)
        target_compile_definitions(MKPluginSmokeTest PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_VST3_CAN_REPLACE_VST2=0
        )
        target_link_libraries(MKPluginSmokeTest PRIVATE
            mkambient_core
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
        )
    endif()

    if (MK_TARGET_IOS)
        # AUv3 app extension + host app. Touch-first layout, no microphone.
        target_compile_definitions(MKAmbient PUBLIC
            JUCE_HAS_LIBMP3 FALSE
            JUCE_IOS TRUE
        )
        # juce_add_plugin generates a separate SharedCode target plus one target per
        # format, so the build settings have to be applied to the app and the appex
        # as well - setting them on MKAmbient alone leaves the app shipping as
        # iPhone+iPad with a default (Os) optimisation level.
        foreach (target MKAmbient MKAmbient_Standalone MKAmbient_AUv3)
            if (TARGET ${target})
                set_target_properties(${target} PROPERTIES
                    # iPad only: the interface is laid out for a tablet and the
                    # iPhone form factor would only shrink it.
                    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "2"
                    XCODE_ATTRIBUTE_SUPPORTS_MACCATALYST "NO"
                    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
                    # Ship one slice: every iPad that runs iPadOS 17 is arm64, and fat
                    # binaries only cost download size and dyld work.
                    XCODE_ATTRIBUTE_ARCHS "arm64"
                    XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO"
                    XCODE_ATTRIBUTE_VALID_ARCHS "arm64"
                    XCODE_ATTRIBUTE_ENABLE_TESTABILITY "NO"
                    XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING "YES"
                    XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL "3"
                    XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN "YES"
                    XCODE_ATTRIBUTE_CLANG_LTO "YES"
                )
            endif()
        endforeach()
    endif()
endif()

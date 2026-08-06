cmake_minimum_required(VERSION 3.22)

project(NDLR_JUCE VERSION 0.1.1 LANGUAGES C CXX)

include(CTest)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(JUCE_DIR "/Applications/JUCE" CACHE PATH "Path to the JUCE source tree")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found. Set JUCE_DIR to the JUCE source directory.")
endif()

add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(NDLR
    COMPANY_NAME "Patrick Pirat"
    BUNDLE_ID "com.patrickpirat.ndlr"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/32.png"
    PLUGIN_MANUFACTURER_CODE Ppir
    PLUGIN_CODE Ndlr
    FORMATS Standalone AU VST3
    PRODUCT_NAME "NDLR"
    IS_SYNTH FALSE
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    # Ableton Live rejects VST3 plug-ins advertised as effects when they have
    # no audio input bus. NDLR is a MIDI generator, so expose it as a silent
    # instrument/generator in VST3 hosts while keeping the AU MIDI-processor
    # type selected by IS_MIDI_EFFECT.
    VST3_CATEGORIES Instrument Generator
    # Keep the plug-in folders in sync with the binaries that have just been built.
    # Otherwise hosts continue to load an older AU/VST3 while only the Standalone
    # application reflects the latest source and layout changes.
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(NDLR)

# The exported editor state is compiled into the plug-in and provides the
# default UI layout for new instances. Saved projects can still override it.
juce_add_binary_data(NDLRDefaultLayout
    NAMESPACE ndlr_resources
    HEADER_NAME NdlrDefaultLayoutData.h
    SOURCES
        ndlr-state
        Resources/SpaceAge.ttf
)

# Layout Mode stores its latest geometry in the Standalone state and in a
# small cache file. Merge it before BinaryData is generated so a regular build
# always embeds the most recently edited layout.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(NDLRSyncLayout
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/Tools/sync_layout_state.py"
                --state "${CMAKE_CURRENT_SOURCE_DIR}/ndlr-state"
        VERBATIM
    )
    add_dependencies(NDLRDefaultLayout NDLRSyncLayout)
endif()

target_sources(NDLR PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/UI/NdlrLookAndFeel.h
    Source/UI/HarmonyWheel.cpp
    Source/UI/HarmonyWheel.h
    Source/UI/RhythmWheel.cpp
    Source/UI/RhythmWheel.h
    Source/UI/PatternDisplay.h
    Source/UI/PadVoicingPreview.h
    Source/UI/LayoutDebugOverlay.cpp
    Source/UI/LayoutDebugOverlay.h
    Source/UI/ModulationScope.h
    Source/UI/PerlinDisplay.h
    Source/Engine/NdlrEngine.cpp
    Source/Engine/NdlrEngine.h
    Source/Engine/RhythmUtilities.h
    Source/Engine/TimingUtilities.h
    Source/Engine/PerlinNoise.h
)

target_compile_definitions(NDLR PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NDLR PRIVATE
    NDLRDefaultLayout
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(BUILD_TESTING)
    juce_add_console_app(NDLRTests PRODUCT_NAME "NDLR Tests")
    juce_generate_juce_header(NDLRTests)
    target_sources(NDLRTests PRIVATE
        Tests/NdlrTests.cpp
        Source/Engine/NdlrEngine.cpp
        Source/Engine/NdlrEngine.h
        Source/Engine/RhythmUtilities.h
        Source/Engine/TimingUtilities.h
    )
    target_include_directories(NDLRTests PRIVATE Source)
    target_link_libraries(NDLRTests PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(NDLRTests PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
            $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        )
        target_link_options(NDLRTests PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        )
    endif()
    add_test(NAME NDLR.EngineRegression COMMAND NDLRTests)
endif()

option(NDLR_ADHOC_SIGN_BUNDLES "Ad-hoc sign complete local macOS bundles after generation" ON)
if(APPLE AND NDLR_ADHOC_SIGN_BUNDLES)
    foreach(bundle_target NDLR_Standalone NDLR_AU NDLR_VST3)
        add_custom_command(TARGET ${bundle_target} POST_BUILD
            COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:${bundle_target}>"
            COMMENT "Ad-hoc signing the complete ${bundle_target} bundle"
            VERBATIM
        )
    endforeach()
endif()

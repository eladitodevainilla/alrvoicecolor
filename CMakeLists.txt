cmake_minimum_required(VERSION 3.22)

project(ALRVoiceColor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ALR_BUILD_PLUGIN "Build JUCE Audio Plugin targets (VST3 / AU / Standalone)" ON)
option(ALR_BUILD_TESTS  "Build standalone C++20 DSP verification test suite" ON)

# ----------------------------------------------------------------------------
# Core DSP Static Library (Zero JUCE dependencies — fast, portable, testable)
# ----------------------------------------------------------------------------
add_library(ALRVoiceColorDSP STATIC
    Source/DSP/DSPCommon.h
    Source/DSP/VoiceProfile.h
    Source/DSP/VoiceActivityDetector.h
    Source/DSP/VoiceActivityDetector.cpp
    Source/DSP/VoiceAnalyzer.h
    Source/DSP/VoiceAnalyzer.cpp
    Source/DSP/PitchDetector.h
    Source/DSP/PitchDetector.cpp
    Source/DSP/PitchCorrector.h
    Source/DSP/PitchCorrector.cpp
    Source/DSP/VocalEqualizer.h
    Source/DSP/VocalEqualizer.cpp
    Source/DSP/ResonanceController.h
    Source/DSP/ResonanceController.cpp
    Source/DSP/HarmonicSaturation.h
    Source/DSP/HarmonicSaturation.cpp
    Source/DSP/DynamicProcessor.h
    Source/DSP/DynamicProcessor.cpp
    Source/DSP/VocalProcessingChain.h
    Source/DSP/VocalProcessingChain.cpp
)

target_include_directories(ALRVoiceColorDSP PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
set_target_properties(ALRVoiceColorDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(ALRVoiceColorDSP PRIVATE /W4 /O2)
else()
    target_compile_options(ALRVoiceColorDSP PRIVATE -Wall -Wextra -O2)
endif()

# ----------------------------------------------------------------------------
# Standalone DSP Unit Test Suite (No JUCE required)
# ----------------------------------------------------------------------------
if(ALR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

# ----------------------------------------------------------------------------
# JUCE 8 Plugin Targets (VST3, AU on macOS, Standalone)
# ----------------------------------------------------------------------------
if(ALR_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    set(ALR_PLUGIN_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND ALR_PLUGIN_FORMATS AU)
    endif()

    juce_add_plugin(ALRVoiceColor
        COMPANY_NAME                "ALR Audio"
        COMPANY_WEBSITE             "https://alrworldwide.com"
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        PLUGIN_MANUFACTURER_CODE    ALRA
        PLUGIN_CODE                 ALVC
        FORMATS                     ${ALR_PLUGIN_FORMATS}
        PRODUCT_NAME                "ALR VOICE COLOR"
        VST3_CATEGORIES             Fx EQ Dynamics "Pitch Shift"
        AU_MAIN_TYPE                kAudioUnitType_Effect
    )

    target_sources(ALRVoiceColor PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
        Source/Parameters/ParameterIDs.h
        Source/Parameters/ParameterManager.h
        Source/Parameters/ParameterManager.cpp
        Source/UI/ALRLookAndFeel.h
        Source/UI/ALRLookAndFeel.cpp
        Source/UI/RotaryKnobComponent.h
        Source/UI/RotaryKnobComponent.cpp
        Source/UI/MeterComponent.h
        Source/UI/MeterComponent.cpp
        Source/UI/SpectrumDisplay.h
        Source/UI/SpectrumDisplay.cpp
        Source/UI/RelearnPanel.h
        Source/UI/RelearnPanel.cpp
        Source/UI/VoiceColorPanel.h
        Source/UI/VoiceColorPanel.cpp
        Source/UI/PitchPanel.h
        Source/UI/PitchPanel.cpp
    )

    target_compile_definitions(ALRVoiceColor PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
    )

    target_link_libraries(ALRVoiceColor PRIVATE
        ALRVoiceColorDSP
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )
endif()

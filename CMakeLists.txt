cmake_minimum_required(VERSION 3.22)
project(DopplerSat VERSION 1.14.0)

# JUCE 9.0.3 ligger som soesterfolder til repoet (samme som Multiband Saturator)
add_subdirectory(../JUCE JUCE)

# Hver installeret version er sit EGET plugin i Ableton (eget navn og egen plugin-kode), saa de gamle
# versioner stadig kan bruges ved siden af. Hoej minor-versionen ovenfor foer hver ny installation.
set(DS_VERSION_NAME "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
if (PROJECT_VERSION_MINOR LESS 10)
    set(DS_PLUGIN_CODE "D${PROJECT_VERSION_MAJOR}0${PROJECT_VERSION_MINOR}")
else()
    set(DS_PLUGIN_CODE "D${PROJECT_VERSION_MAJOR}${PROJECT_VERSION_MINOR}")
endif()

# 1.5: paa Mac ogsaa Audio Unit (Logic kan kun AU). Bygges paa en Mac eller i GitHub Actions
# (.github/workflows/mac.yml); paa Windows kun VST3 som foer.
if (APPLE)
    set(DS_FORMATS VST3 AU)
else()
    set(DS_FORMATS VST3)
endif()

juce_add_plugin(DopplerSat
    COMPANY_NAME "DitNavn"
    PLUGIN_MANUFACTURER_CODE Dtnv
    PLUGIN_CODE ${DS_PLUGIN_CODE}
    FORMATS ${DS_FORMATS}
    # 1.3: nyt navn "Pitch Bend Texture" (1.0-1.2 hed "Doppler Saturator"). Maal og mappe hedder stadig DopplerSat.
    PRODUCT_NAME "Pitch Bend Texture ${DS_VERSION_NAME}"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(DopplerSat)

target_sources(DopplerSat PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SatDsp.h
    Source/SimdShim.h
    Source/DopplerCore.h)

target_compile_definitions(DopplerSat PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

if (MSVC)
    target_compile_options(DopplerSat PRIVATE /utf-8)
else()
    # Ingen FMA (gange + plus i eet), saa SimdShim.h paa Apple Silicon regner bit for bit som SSE2 paa Windows
    target_compile_options(DopplerSat PRIVATE -ffp-contract=off)
endif()

# Test: SimdShim.h i stedet for SSE2 ogsaa paa Windows (cmake -DDS_FORCE_SIMD_SHIM=ON), skal give samme lyd
option(DS_FORCE_SIMD_SHIM "SatDsp.h uden SSE2 (som paa Apple Silicon)" OFF)
if (DS_FORCE_SIMD_SHIM)
    target_compile_definitions(DopplerSat PUBLIC DS_FORCE_SIMD_SHIM=1)
endif()

target_link_libraries(DopplerSat PRIVATE
    juce::juce_audio_utils juce::juce_dsp juce::juce_audio_processors
    juce::juce_recommended_config_flags)

cmake_minimum_required(VERSION 3.22)
project(DopplerSat VERSION 1.3.0)

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

juce_add_plugin(DopplerSat
    COMPANY_NAME "DitNavn"
    PLUGIN_MANUFACTURER_CODE Dtnv
    PLUGIN_CODE ${DS_PLUGIN_CODE}
    FORMATS VST3
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
    Source/DopplerCore.h)

target_compile_definitions(DopplerSat PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

if (MSVC)
    target_compile_options(DopplerSat PRIVATE /utf-8)
endif()

target_link_libraries(DopplerSat PRIVATE
    juce::juce_audio_utils juce::juce_dsp juce::juce_audio_processors
    juce::juce_recommended_config_flags)

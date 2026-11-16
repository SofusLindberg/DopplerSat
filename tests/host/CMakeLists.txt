cmake_minimum_required(VERSION 3.22)
project(DopplerHost VERSION 1.0.0)
add_subdirectory(../../../JUCE JUCE)

# Roeg-test: indlaes den byggede VST3 som en vaert: HostSmoke.exe "<.vst3>"
juce_add_console_app(HostSmoke PRODUCT_NAME "HostSmoke")
target_sources(HostSmoke PRIVATE main.cpp)
target_compile_definitions(HostSmoke PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
if (APPLE)
    # 1.5: paa Mac ogsaa Audio Units (HostSmoke "AU:Pitch Bend Texture 1.x"), samme vej som Logic
    target_compile_definitions(HostSmoke PRIVATE JUCE_PLUGINHOST_AU=1)
endif()
target_link_libraries(HostSmoke PRIVATE juce::juce_audio_processors juce::juce_audio_utils juce::juce_recommended_config_flags)
juce_generate_juce_header(HostSmoke)

# Billeder af editoren uden vaert (kompilerer pluginnets kilder direkte): EditorShot.exe <mappe>
juce_add_console_app(EditorShot PRODUCT_NAME "EditorShot")
target_sources(EditorShot PRIVATE editor_shot.cpp ../../Source/PluginProcessor.cpp ../../Source/PluginEditor.cpp ../../Source/License.cpp)
# DS_LICENSE_TESTING: licensen kan saettes til/fra af testen (kun i test-programmer, aldrig i pluginnet)
target_compile_definitions(EditorShot PRIVATE JucePlugin_Name="EditorShot" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 DS_LICENSE_TESTING=1)
if (MSVC)
    target_compile_options(EditorShot PRIVATE /utf-8)
endif()
target_link_libraries(EditorShot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_audio_processors juce::juce_product_unlocking juce::juce_recommended_config_flags)
juce_generate_juce_header(EditorShot)

# Hvad koster hvad (uden vaert): DspBench.exe
juce_add_console_app(DspBench PRODUCT_NAME "DspBench")
target_sources(DspBench PRIVATE dsp_bench.cpp ../../Source/PluginProcessor.cpp ../../Source/PluginEditor.cpp ../../Source/License.cpp)
target_compile_definitions(DspBench PRIVATE JucePlugin_Name="DspBench" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 DS_LICENSE_TESTING=1)
if (MSVC)
    target_compile_options(DspBench PRIVATE /utf-8)
endif()
target_link_libraries(DspBench PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_audio_processors juce::juce_product_unlocking juce::juce_recommended_config_flags)
juce_generate_juce_header(DspBench)

# 2.08 licens: noegler og den lokale test-licensserver (LicenseTool) og hele licensforloebet (LicenseTest)
foreach (tool LicenseTool LicenseTest)
    juce_add_console_app(${tool} PRODUCT_NAME "${tool}")
    target_compile_definitions(${tool} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if (MSVC)
        target_compile_options(${tool} PRIVATE /utf-8)
    endif()
    target_link_libraries(${tool} PRIVATE juce::juce_core juce::juce_events juce::juce_data_structures juce::juce_cryptography
                                          juce::juce_product_unlocking juce::juce_recommended_config_flags)
    juce_generate_juce_header(${tool})
endforeach()
target_sources(LicenseTool PRIVATE license_tool.cpp license_server.cpp ../../Source/License.cpp)
target_sources(LicenseTest PRIVATE license_test.cpp license_server.cpp ../../Source/License.cpp)

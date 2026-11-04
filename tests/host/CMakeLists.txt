cmake_minimum_required(VERSION 3.22)
project(DopplerHost VERSION 1.0.0)
add_subdirectory(../../../JUCE JUCE)

# Roeg-test: indlaes den byggede VST3 som en vaert: HostSmoke.exe "<.vst3>"
juce_add_console_app(HostSmoke PRODUCT_NAME "HostSmoke")
target_sources(HostSmoke PRIVATE main.cpp)
target_compile_definitions(HostSmoke PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(HostSmoke PRIVATE juce::juce_audio_processors juce::juce_audio_utils juce::juce_recommended_config_flags)
juce_generate_juce_header(HostSmoke)

# Billeder af editoren uden vaert (kompilerer pluginnets kilder direkte): EditorShot.exe <mappe>
juce_add_console_app(EditorShot PRODUCT_NAME "EditorShot")
target_sources(EditorShot PRIVATE editor_shot.cpp ../../Source/PluginProcessor.cpp ../../Source/PluginEditor.cpp)
target_compile_definitions(EditorShot PRIVATE JucePlugin_Name="EditorShot" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
if (MSVC)
    target_compile_options(EditorShot PRIVATE /utf-8)
endif()
target_link_libraries(EditorShot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_audio_processors juce::juce_recommended_config_flags)
juce_generate_juce_header(EditorShot)

# Hvad koster hvad (uden vaert): DspBench.exe
juce_add_console_app(DspBench PRODUCT_NAME "DspBench")
target_sources(DspBench PRIVATE dsp_bench.cpp ../../Source/PluginProcessor.cpp ../../Source/PluginEditor.cpp)
target_compile_definitions(DspBench PRIVATE JucePlugin_Name="DspBench" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
if (MSVC)
    target_compile_options(DspBench PRIVATE /utf-8)
endif()
target_link_libraries(DspBench PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_audio_processors juce::juce_recommended_config_flags)
juce_generate_juce_header(DspBench)

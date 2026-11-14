# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# Unit test for src/common/MidiMap.h (CC map, learn, soft takeover, tap, state, no allocation in process). Not part of the normal build:
#   cmake --build ~/Developer/VIBR --target MidiMapTest && ~/Developer/VIBR/tools/midi-test/MidiMapTest_artefacts/Release/MidiMapTest
# Exit code 0 = all checks passed.
juce_add_console_app(MidiMapTest PRODUCT_NAME "MidiMapTest")
juce_generate_juce_header(MidiMapTest)
target_sources(MidiMapTest PRIVATE ${CMAKE_CURRENT_LIST_DIR}/MidiMapTest.cpp)
target_compile_definitions(MidiMapTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(MidiMapTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(MidiMapTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

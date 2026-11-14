# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# RB-1 Reverb Lab: classic (BS-1), space (SP-1) and resonating strings (SR-1) reverbs (ReverbLabEngine.h). The window is drawn in code (no background image).
juce_add_plugin(RB-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.rb1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Rb01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "RB-1 Reverb Lab"
)

juce_generate_juce_header(RB-1)

target_sources(RB-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    ReverbLabEngine.h
)

target_compile_definitions(RB-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(RB-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(RB-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target RB1EngineTest
juce_add_console_app(RB1EngineTest PRODUCT_NAME "RB1EngineTest")
juce_generate_juce_header(RB1EngineTest)
target_sources(RB1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(RB1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(RB1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(RB1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

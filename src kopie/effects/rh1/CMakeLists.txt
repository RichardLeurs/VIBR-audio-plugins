# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# RH-1 Rhythm Lab: filter (MF-1), tremolo (TR-2) and slicer (SL-2) destinations of one control engine (RhythmEngine.h). The window is drawn in code (no background image).
juce_add_plugin(RH-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.rh1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Rh01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "RH-1 Rhythm Lab"
)

juce_generate_juce_header(RH-1)

target_sources(RH-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    RhythmEngine.h
)

target_compile_definitions(RH-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(RH-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(RH-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target RH1EngineTest
juce_add_console_app(RH1EngineTest PRODUCT_NAME "RH1EngineTest")
juce_generate_juce_header(RH1EngineTest)
target_sources(RH1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(RH1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(RH1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(RH1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

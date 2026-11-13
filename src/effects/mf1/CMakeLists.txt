# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# MF-1 Motion Filter: a filter moved by an LFO, an envelope or a step sequencer (MotionEngine.h). The window is drawn in code (no background image).
juce_add_plugin(MF-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.mf1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Mf01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "MF-1 Motion Filter"
)

juce_generate_juce_header(MF-1)

target_sources(MF-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    MotionEngine.h
)

target_compile_definitions(MF-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MF-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(MF-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target MF1EngineTest
juce_add_console_app(MF1EngineTest PRODUCT_NAME "MF1EngineTest")
juce_generate_juce_header(MF1EngineTest)
target_sources(MF1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(MF1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(MF1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(MF1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

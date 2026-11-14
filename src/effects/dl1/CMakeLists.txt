# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# DL-1 Delay Lab: digital (SD-1) and analog (MM-1) delay voicings, more to come (DelayEngine.h). The window is drawn in code (no background image).
juce_add_plugin(DL-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.dl1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Dl01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "DL-1 Delay Lab"
)

juce_generate_juce_header(DL-1)

target_sources(DL-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    DelayEngine.h
)

target_compile_definitions(DL-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DL-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(DL-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target DL1EngineTest
juce_add_console_app(DL1EngineTest PRODUCT_NAME "DL1EngineTest")
juce_generate_juce_header(DL1EngineTest)
target_sources(DL1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(DL1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(DL1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(DL1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

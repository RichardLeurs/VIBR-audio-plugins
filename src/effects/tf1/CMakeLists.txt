# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# TF-1 Time Fracture: a pattern of stutters, reversals, scrambles, pitch jumps and stretches over a rolling buffer (TimeEngine.h). The window is drawn in code.
juce_add_plugin(TF-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.tf1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Tf01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "TF-1 Time Fracture"
)

juce_generate_juce_header(TF-1)

target_sources(TF-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    TimeEngine.h
)

target_compile_definitions(TF-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(TF-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(TF-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target TF1EngineTest
juce_add_console_app(TF1EngineTest PRODUCT_NAME "TF1EngineTest")
juce_generate_juce_header(TF1EngineTest)
target_sources(TF1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(TF1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(TF1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
set_target_properties(TF1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

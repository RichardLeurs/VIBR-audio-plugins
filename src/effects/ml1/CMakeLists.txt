# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# ML-1 Modulation Lab: chorus, vibrato, flanger and phaser (ModEngine.h). The window is drawn in code (no background image).
juce_add_plugin(ML-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.ml1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Ml01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "ML-1 Modulation Lab"
)

juce_generate_juce_header(ML-1)

target_sources(ML-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    ModEngine.h
)

target_compile_definitions(ML-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ML-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(ML-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target ML1EngineTest
juce_add_console_app(ML1EngineTest PRODUCT_NAME "ML1EngineTest")
juce_generate_juce_header(ML1EngineTest)
target_sources(ML1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(ML1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ML1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(ML1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

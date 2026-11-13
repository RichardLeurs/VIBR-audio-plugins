# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# GR-1 Grain: granular processing: grains cut from the playing, with pitch, position and density control, plus freeze (GrainEngine.h). The window is drawn in code (no background image).
juce_add_plugin(GR-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.gr1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Gr01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "GR-1 Grain"
)

juce_generate_juce_header(GR-1)

target_sources(GR-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    GrainEngine.h
)

target_compile_definitions(GR-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(GR-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(GR-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target GR1EngineTest
juce_add_console_app(GR1EngineTest PRODUCT_NAME "GR1EngineTest")
juce_generate_juce_header(GR1EngineTest)
target_sources(GR1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(GR1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(GR1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(GR1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

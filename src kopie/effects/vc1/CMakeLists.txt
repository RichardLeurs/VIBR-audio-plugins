# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# VC-1 Voltage Controller: a modulation matrix (envelope, two LFOs, random, MIDI) on filter, amp, pitch and pan (ModEngine.h). The window is drawn in code (no background image).
juce_add_plugin(VC-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.vc1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Vc01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "VC-1 Voltage Controller"
)

juce_generate_juce_header(VC-1)

target_sources(VC-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    ModEngine.h
)

target_compile_definitions(VC-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VC-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(VC-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target VC1EngineTest
juce_add_console_app(VC1EngineTest PRODUCT_NAME "VC1EngineTest")
juce_generate_juce_header(VC1EngineTest)
target_sources(VC1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(VC1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(VC1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(VC1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

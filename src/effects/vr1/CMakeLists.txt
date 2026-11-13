# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# VR-1 Vibe and Rotary: uni-vibe, rotary speaker and ring mod (VibeEngine.h). The window is drawn in code (no background image).
juce_add_plugin(VR-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.vr1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Vr01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "VR-1 Vibe and Rotary"
)

juce_generate_juce_header(VR-1)

target_sources(VR-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    VibeEngine.h
)

target_compile_definitions(VR-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VR-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(VR-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target VR1EngineTest
juce_add_console_app(VR1EngineTest PRODUCT_NAME "VR1EngineTest")
juce_generate_juce_header(VR1EngineTest)
target_sources(VR1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(VR1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(VR1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(VR1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# SU-1 Stereo Utility: channels, width, pan and level with a stereo field display (UtilityEngine.h). The window is drawn in code (no background image).
juce_add_plugin(SU-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.su1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Su01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "SU-1 Stereo Utility"
)

juce_generate_juce_header(SU-1)

target_sources(SU-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    UtilityEngine.h
)

target_compile_definitions(SU-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SU-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(SU-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target SU1EngineTest
juce_add_console_app(SU1EngineTest PRODUCT_NAME "SU1EngineTest")
juce_generate_juce_header(SU1EngineTest)
target_sources(SU1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(SU1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(SU1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(SU1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# DV-1 Drive Lab: overdrive (OD-2), distortion (DS-1), tube, tape, fold and crush voicings (DriveEngine.h). The window is drawn in code (no background image).
juce_add_plugin(DV-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.dv1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Dv01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "DV-1 Drive Lab"
)

juce_generate_juce_header(DV-1)

target_sources(DV-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    DriveEngine.h
)

target_compile_definitions(DV-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DV-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(DV-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target DV1EngineTest
juce_add_console_app(DV1EngineTest PRODUCT_NAME "DV1EngineTest")
juce_generate_juce_header(DV1EngineTest)
target_sources(DV1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(DV1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(DV1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
set_target_properties(DV1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

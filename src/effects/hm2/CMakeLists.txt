# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# HM-2 Harmonic Machine: adds and reshapes harmonics (HarmonicEngine.h). The window is drawn in code (no background image).
juce_add_plugin(HM-2
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.hm2
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Hm02
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "HM-2 Harmonic Machine"
)

juce_generate_juce_header(HM-2)

target_sources(HM-2
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    HarmonicEngine.h
)

target_compile_definitions(HM-2
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(HM-2
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(HM-2)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target HM2EngineTest
juce_add_console_app(HM2EngineTest PRODUCT_NAME "HM2EngineTest")
juce_generate_juce_header(HM2EngineTest)
target_sources(HM2EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(HM2EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(HM2EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(HM2EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# EQ-1 Graphic EQ (test plugin): the 8-band equalizer of the MS-1 (src/mastering/ms1/MasteringEngine.h, ms1::Eq) with its interactive curve display as a plugin of its own.
# The window is drawn in code (no background image).
juce_add_plugin(EQ-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.eq1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Eq01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "EQ-1 Graphic EQ"
)

juce_generate_juce_header(EQ-1)

target_sources(EQ-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(EQ-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EQ-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(EQ-1)

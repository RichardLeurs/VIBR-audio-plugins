# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
juce_add_binary_data(BS1Assets
  HEADER_NAME BS1Data.h
  NAMESPACE BS1Data
  SOURCES
    assets/background.png
)

juce_add_plugin(BS-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.bs1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Bgsk
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "BS-1 Reverb"
)

juce_generate_juce_header(BS-1)

target_sources(BS-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(BS-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(BS-1
  PRIVATE
    rl_common
    BS1Assets
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(BS-1)

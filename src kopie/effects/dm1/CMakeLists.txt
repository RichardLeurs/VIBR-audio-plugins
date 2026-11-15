# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
juce_add_binary_data(DM1Assets
  HEADER_NAME DM1Data.h
  NAMESPACE DM1Data
  SOURCES
    assets/background.png
)

juce_add_plugin(DM-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.dm1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Dm10
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "DM-1 Delay Machine"
)

juce_generate_juce_header(DM-1)

target_sources(DM-1
  PRIVATE
    DM1DSP.h
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(DM-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DM-1
  PRIVATE
    rl_common
    DM1Assets
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(DM-1)

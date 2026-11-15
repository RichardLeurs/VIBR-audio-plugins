# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
juce_add_binary_data(OC5Assets
  HEADER_NAME OC5Data.h
  NAMESPACE OC5Data
  SOURCES
    assets/background.png
)

juce_add_plugin(OC-5
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.oc5
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Oc05
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "OC-5 Octave"
)

juce_generate_juce_header(OC-5)

target_sources(OC-5
  PRIVATE
    OC5DSP.h
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(OC-5
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(OC-5
  PRIVATE
    OC5Assets
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(OC-5)

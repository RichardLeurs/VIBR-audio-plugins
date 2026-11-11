# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
juce_add_binary_data(DS1Assets
  HEADER_NAME DS1Data.h
  NAMESPACE DS1Data
  SOURCES
    assets/background.png
)

juce_add_plugin(DS-1
  COMPANY_NAME "Richard Leurs"
  BUNDLE_ID com.richardleurs.ds1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Ds01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "DS-1 Distortion"
)

juce_generate_juce_header(DS-1)

target_sources(DS-1
  PRIVATE
    DS1DSP.h
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(DS-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DS-1
  PRIVATE
    DS1Assets
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(DS-1)

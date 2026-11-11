# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
juce_add_binary_data(CE1Assets
  HEADER_NAME CE1Data.h
  NAMESPACE CE1Data
  SOURCES
    assets/background.png
)

juce_add_plugin(CE-1
  COMPANY_NAME "Richard Leurs"
  BUNDLE_ID com.richardleurs.ce1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Ce01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "CE-1 Chorus Ensemble"
)

juce_generate_juce_header(CE-1)

target_sources(CE-1
  PRIVATE
    CE1DSP.h
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(CE-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(CE-1
  PRIVATE
    CE1Assets
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(CE-1)

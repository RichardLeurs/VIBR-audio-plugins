# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
juce_add_binary_data(TR08Assets
  HEADER_NAME TR08Data.h
  NAMESPACE TR08Data
  SOURCES
    assets/background.png
)

juce_add_plugin(TR-8
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.tr08
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Tr08
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "TR-8 Drum Machine"
)

juce_generate_juce_header(TR-8)

target_sources(TR-8
  PRIVATE
    TR08DSP.h
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(TR-8
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(TR-8
  PRIVATE
    TR08Assets
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(TR-8)

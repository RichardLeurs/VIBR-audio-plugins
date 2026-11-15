# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
juce_add_binary_data(EH2Assets
  HEADER_NAME EH2Data.h
  NAMESPACE EH2Data
  SOURCES
    assets/background.png
)

juce_add_plugin(EH-2
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.eh2
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Eh02
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "EH-2 Enhancer"
)

juce_generate_juce_header(EH-2)

target_sources(EH-2
  PRIVATE
    EH2DSP.h
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(EH-2
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EH-2
  PRIVATE
    EH2Assets
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(EH-2)

# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
juce_add_binary_data(LM2Assets
  HEADER_NAME LM2Data.h
  NAMESPACE LM2Data
  SOURCES
    assets/background.png
)

juce_add_plugin(LM-2
  COMPANY_NAME "Richard Leurs"
  BUNDLE_ID com.richardleurs.lm2
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Lm02
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "LM-2 Limiter"
)

juce_generate_juce_header(LM-2)

target_sources(LM-2
  PRIVATE
    LM2DSP.h
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(LM-2
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LM-2
  PRIVATE
    LM2Assets
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(LM-2)

# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
juce_add_binary_data(RE1Assets
  HEADER_NAME RE1Data.h
  NAMESPACE RE1Data
  SOURCES
    assets/background.png
)

juce_add_plugin(RE-1
  COMPANY_NAME "Richard Leurs"
  BUNDLE_ID com.richardleurs.re1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Spce
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "RE-1 Space Echo"
)

juce_generate_juce_header(RE-1)

target_sources(RE-1
  PRIVATE
    SpaceEchoDSP.h
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(RE-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(RE-1
  PRIVATE
    RE1Assets
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(RE-1)

# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
juce_add_binary_data(SD3Assets
  HEADER_NAME SD3Data.h
  NAMESPACE SD3Data
  SOURCES
    assets/background.png
)

juce_add_plugin(SD-3
  COMPANY_NAME "Richard Leurs"
  BUNDLE_ID com.richardleurs.sd3
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Sd30
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "SD-3 Digital Delay"
)

juce_generate_juce_header(SD-3)

target_sources(SD-3
  PRIVATE
    SDD3000DSP.h
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(SD-3
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SD-3
  PRIVATE
    SD3Assets
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(SD-3)

# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# SA-1 Spectrum Analyzer (test plugin): a pure analysis plugin, the audio passes untouched (see .claude/spectrum_analyser_specs.md). The window is drawn in code.
juce_add_plugin(SA-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.sa1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE An01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "SA-1 Spectrum Analyzer"
)

juce_generate_juce_header(SA-1)

target_sources(SA-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(SA-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SA-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(SA-1)

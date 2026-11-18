# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# SF-1 Spectral Fold: freeze, shift, smear, morph and fold the spectrum (SpectralEngine.h, a phase vocoder). The window is drawn in code (no background image).
juce_add_plugin(SF-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.sf1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Sf01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "SF-1 Spectral Fold"
)

juce_generate_juce_header(SF-1)

target_sources(SF-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    SpectralEngine.h
)

target_compile_definitions(SF-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SF-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(SF-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target SF1EngineTest
juce_add_console_app(SF1EngineTest PRODUCT_NAME "SF1EngineTest")
juce_generate_juce_header(SF1EngineTest)
target_sources(SF1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(SF1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(SF1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
set_target_properties(SF1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

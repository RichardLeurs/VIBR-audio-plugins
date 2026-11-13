# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# SP-1 Space Engine: shimmer, infinite, spectral, modulated and room spaces (SpaceEngine.h). The window is drawn in code (no background image).
juce_add_plugin(SP-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.sp1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Sp01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "SP-1 Space Engine"
)

juce_generate_juce_header(SP-1)

target_sources(SP-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    SpaceEngine.h
)

target_compile_definitions(SP-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SP-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(SP-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target SP1EngineTest
juce_add_console_app(SP1EngineTest PRODUCT_NAME "SP1EngineTest")
juce_generate_juce_header(SP1EngineTest)
target_sources(SP1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(SP1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(SP1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(SP1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# DN-1 Dynamics Lab: shaper (DY-1), compressor (CS-3 and more) and limiter (LM-2, true peak) voicings (DynamicsEngine.h). The window is drawn in code (no background image).
juce_add_plugin(DN-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.dn1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Dn01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "DN-1 Dynamics Lab"
)

juce_generate_juce_header(DN-1)

target_sources(DN-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    DynamicsEngine.h
)

target_compile_definitions(DN-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DN-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(DN-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target DN1EngineTest
juce_add_console_app(DN1EngineTest PRODUCT_NAME "DN1EngineTest")
juce_generate_juce_header(DN1EngineTest)
target_sources(DN1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(DN1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(DN1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(DN1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# DY-1 Dynamic Shaper: noise gate, expander and transient shaper in one chain (DynamicEngine.h). The window is drawn in code (no background image).
juce_add_plugin(DY-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.dy1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Dy01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "DY-1 Dynamic Shaper"
)

juce_generate_juce_header(DY-1)

target_sources(DY-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    DynamicEngine.h
)

target_compile_definitions(DY-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DY-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(DY-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target DY1EngineTest
juce_add_console_app(DY1EngineTest PRODUCT_NAME "DY1EngineTest")
juce_generate_juce_header(DY1EngineTest)
target_sources(DY1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(DY1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(DY1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(DY1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

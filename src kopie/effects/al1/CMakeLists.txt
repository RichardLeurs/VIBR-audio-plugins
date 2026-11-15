# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# AL-1 Amp Lab: Raw Power. 14 amp types, gate, tight boost, 4x oversampled amp stages, two impulse-response cabinets or a filter model, EQ, ambience, limiter
# (AL1Engine.h, .claude/ampcabinet_spec.md). The window is drawn in code (no background image).
juce_add_plugin(AL-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.al1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Al01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "AL-1 Amp Lab"
)

juce_generate_juce_header(AL-1)

target_sources(AL-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    AL1Engine.h
)

target_compile_definitions(AL-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AL-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(AL-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target AL1EngineTest
juce_add_console_app(AL1EngineTest PRODUCT_NAME "AL1EngineTest")
juce_generate_juce_header(AL1EngineTest)
target_sources(AL1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(AL1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(AL1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
set_target_properties(AL1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

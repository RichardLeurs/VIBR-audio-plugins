# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# PT-1 Phantom: a four-voice harmonizer with key and scale aware (intelligent) harmonies (PhantomEngine.h). The window is drawn in code (no background image).
juce_add_plugin(PT-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.pt1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Pt01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "PT-1 Phantom"
)

juce_generate_juce_header(PT-1)

target_sources(PT-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    PhantomEngine.h
)

target_compile_definitions(PT-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(PT-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(PT-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target PT1EngineTest
juce_add_console_app(PT1EngineTest PRODUCT_NAME "PT1EngineTest")
juce_generate_juce_header(PT1EngineTest)
target_sources(PT1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(PT1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(PT1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(PT1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

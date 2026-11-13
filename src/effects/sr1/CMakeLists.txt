# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# SR-1 Sympathetic Resonator: virtual strings that ring along with the playing (ResonatorEngine.h). The window is drawn in code (no background image).
juce_add_plugin(SR-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.sr1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Sr01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "SR-1 Sympathetic Resonator"
)

juce_generate_juce_header(SR-1)

target_sources(SR-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    ResonatorEngine.h
)

target_compile_definitions(SR-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SR-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(SR-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target SR1EngineTest
juce_add_console_app(SR1EngineTest PRODUCT_NAME "SR1EngineTest")
juce_generate_juce_header(SR1EngineTest)
target_sources(SR1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(SR1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(SR1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(SR1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# ME-1 multi effect: a chain of up to 8 basic effect modules (Modules.cpp). Self-contained; it does not use the pedals in src/effects/.
juce_add_plugin(ME-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.me1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Me01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "ME-1 Multi Effect"
)

juce_generate_juce_header(ME-1)

target_sources(ME-1
  PRIVATE
    Modules.cpp
    Modules.h
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

target_compile_definitions(ME-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ME-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(ME-1)

# Offline check of the modules (not part of the normal build):  cmake --build <build dir> --target ME1ModuleTest
juce_add_console_app(ME1ModuleTest PRODUCT_NAME "ME1ModuleTest")
juce_generate_juce_header(ME1ModuleTest)
target_sources(ME1ModuleTest PRIVATE ModuleTest.cpp Modules.cpp)
target_compile_definitions(ME1ModuleTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ME1ModuleTest PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(ME1ModuleTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

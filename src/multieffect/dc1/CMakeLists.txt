# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# DC-1 multi-effect chain. It hosts the processors and editors of the pedals in src/effects/: their sources are compiled into
# this target (the pedals' own createPluginFilter() is renamed per pedal so the symbols do not clash).
juce_add_plugin(DC-1
  COMPANY_NAME "Richard Leurs"
  BUNDLE_ID com.richardleurs.dc1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Dc01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "DC-1 Multi Effect"
)

juce_generate_juce_header(DC-1)

target_sources(DC-1
  PRIVATE
    DC1Registry.h
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
)

set(DC1_ASSET_TARGETS "")
foreach(effect IN LISTS EFFECTS)
  string(TOUPPER ${effect} E)
  foreach(f PluginProcessor.cpp PluginEditor.cpp)
    set(src ${CMAKE_SOURCE_DIR}/src/effects/${effect}/${f})
    target_sources(DC-1 PRIVATE ${src})
    set_source_files_properties(${src} PROPERTIES COMPILE_DEFINITIONS "createPluginFilter=createPluginFilter_${effect}")
  endforeach()
  list(APPEND DC1_ASSET_TARGETS ${E}Assets)
endforeach()

target_compile_definitions(DC-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DC-1
  PRIVATE
    ${DC1_ASSET_TARGETS}
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(DC-1)

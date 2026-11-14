# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# MS-1 Mastering Suite: version 0.1, the foundation (true-peak limiter with a loudness target, loudness / true-peak / spectrum meters, dither). See .claude/mastering_specs.md. The window is drawn in code (no background image).
juce_add_plugin(MS-1
  COMPANY_NAME "VIBR"
  ICON_BIG "${RL_ICON}"
  BUNDLE_ID com.richardleurs.ms1
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Rlrs
  PLUGIN_CODE Ms01
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "MS-1 Mastering Suite"
)

juce_generate_juce_header(MS-1)

target_sources(MS-1
  PRIVATE
    PluginEditor.cpp
    PluginEditor.h
    PluginProcessor.cpp
    PluginProcessor.h
    MasteringStages.h
    ReferencePlayer.h
    MasteringEngine.h
)

target_compile_definitions(MS-1
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MS-1
  PRIVATE
    rl_common
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

copy_vst3_after_build(MS-1)

# Offline check of the engine (not part of the normal build):  cmake --build <build dir> --target MS1EngineTest
juce_add_console_app(MS1EngineTest PRODUCT_NAME "MS1EngineTest")
juce_generate_juce_header(MS1EngineTest)
target_sources(MS1EngineTest PRIVATE EngineTest.cpp)
target_compile_definitions(MS1EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(MS1EngineTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(MS1EngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Offline check of the stages of version 0.4 (crossovers, multiband, imager, colour):  cmake --build <build dir> --target MS1StagesTest
juce_add_console_app(MS1StagesTest PRODUCT_NAME "MS1StagesTest")
juce_generate_juce_header(MS1StagesTest)
target_sources(MS1StagesTest PRIVATE StagesTest.cpp)
target_compile_definitions(MS1StagesTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(MS1StagesTest PRIVATE rl_common juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(MS1StagesTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Check of the reference player (file loading, resampling, analysis, transport):  cmake --build <build dir> --target MS1ReferenceTest
juce_add_console_app(MS1ReferenceTest PRODUCT_NAME "MS1ReferenceTest")
juce_generate_juce_header(MS1ReferenceTest)
target_sources(MS1ReferenceTest PRIVATE ReferenceTest.cpp)
target_compile_definitions(MS1ReferenceTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(MS1ReferenceTest PRIVATE rl_common juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
set_target_properties(MS1ReferenceTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

# The real processor in a console app: instantiates MS1AudioProcessor and checks latency reporting (impulse), NaN / inf input, block sizes 1 .. 8192, every
# sample rate, randomised parameters, repeated prepareToPlay and state round trip:  cmake --build <build dir> --target MS1HostTest
juce_add_console_app(MS1HostTest PRODUCT_NAME "MS1HostTest")
juce_generate_juce_header(MS1HostTest)
target_sources(MS1HostTest PRIVATE HostTest.cpp)
target_compile_definitions(MS1HostTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(MS1HostTest PRIVATE MS-1 rl_common juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
set_target_properties(MS1HostTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

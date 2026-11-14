# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# Screenshot tool for the website: one small app per plugin (each plugin defines createPluginFilter, so they cannot be linked into one):
#   cmake --build ~/Developer/VIBR --target Shot-DY-1
#   ~/Developer/VIBR/tools/screenshot/Shot-DY-1_artefacts/Release/Shot-DY-1.app/Contents/MacOS/Shot-DY-1 dy-1.png stepTone 6 gateOn=1 expOn=0
# Not part of the normal build. See ScreenshotTool.cpp for the signals and the arguments.
function(add_screenshot_tool plugin)
  if(NOT TARGET ${plugin})
    return()
  endif()
  set(tool Shot-${plugin})
  juce_add_gui_app(${tool} PRODUCT_NAME "${tool}")
  juce_generate_juce_header(${tool})
  target_sources(${tool} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ScreenshotTool.cpp)
  target_compile_definitions(${tool} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(${tool} PRIVATE ${plugin} rl_common juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
  set_target_properties(${tool} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endfunction()

foreach(p DY-1 SU-1 SA-1 MS-1 ML-1 VR-1 RV-1 DL-1 DN-1 DV-1 RH-1 RB-1)
  add_screenshot_tool(${p})
endforeach()

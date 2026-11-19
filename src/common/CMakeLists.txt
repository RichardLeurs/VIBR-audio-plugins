# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# Shared, header-only code used by more than one plugin (the reverb engine, the Lab base classes, the UI looks).
add_library(rl_common INTERFACE)
target_include_directories(rl_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# the two fonts of the default look (ui/VintageUI.h), embedded so the look does not depend on installed fonts: Oswald (labels) and IBM Plex Mono (values),
# both SIL Open Font License 1.1 (ui/fonts/OFL-*.txt). Every plugin that links rl_common gets them (a static library: only what a plugin uses ends up in it).
juce_add_binary_data(VibrFonts NAMESPACE VibrFontData HEADER_NAME VibrFontData.h SOURCES
    ui/fonts/OswaldMedium.ttf
    ui/fonts/OswaldSemiBold.ttf
    ui/fonts/OswaldBold.ttf
    ui/fonts/PlexMonoRegular.ttf
    ui/fonts/PlexMonoSemiBold.ttf)
set_target_properties(VibrFonts PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_link_libraries(rl_common INTERFACE VibrFonts)

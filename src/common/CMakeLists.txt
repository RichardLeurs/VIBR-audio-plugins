# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# Shared, header-only DSP used by more than one plugin (currently the reverb engine).
add_library(rl_common INTERFACE)
target_include_directories(rl_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

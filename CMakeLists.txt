# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License

cmake_minimum_required(VERSION 3.22)

# JUCE's bundle step passes DEPENDS to add_custom_command(TARGET); newer CMake warns about it (harmless).
# The default variable (not cmake_policy) reaches JUCE's own directory scope, which resets policies.
set(CMAKE_POLICY_DEFAULT_CMP0175 OLD)

# Lowest macOS the plugins run on. Without this the binaries get this Mac's own OS version as minimum (e.g. 27.0),
# and Ableton on any older macOS silently skips them when scanning. Must be set before project().
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version of the built plugins" FORCE)
endif()

# One CPU architecture per build (smaller bundles than a universal binary): arm64 (Apple Silicon) or x86_64 (Intel).
# Default: the architecture of this Mac. Configure a second build tree for the other one, see build.sh (--arch).
set(RL_ARCH "" CACHE STRING "CPU architecture of this build: arm64 or x86_64 (empty = this Mac's)")
execute_process(COMMAND uname -m OUTPUT_VARIABLE _host_arch OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT RL_ARCH)
  set(RL_ARCH "${_host_arch}")
endif()
if(NOT RL_ARCH STREQUAL _host_arch)       # this Mac's own architecture is the compiler default
  set(CMAKE_OSX_ARCHITECTURES "${RL_ARCH}" CACHE STRING "Architecture of the built plugins (follows RL_ARCH)" FORCE)
endif()

project(VSTWorkspace VERSION 0.1.0)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.3
)

# Prefer a local JUCE checkout outside iCloud-synced folders (files inside ~/Documents can be evicted to the cloud,
# and reading them then stalls the build). Clone once with:
#   git clone --depth 1 --branch 8.0.3 https://github.com/juce-framework/JUCE.git ~/Developer/JUCE
if(NOT FETCHCONTENT_SOURCE_DIR_JUCE AND EXISTS "$ENV{HOME}/Developer/JUCE/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_JUCE "$ENV{HOME}/Developer/JUCE" CACHE PATH "Local JUCE checkout")
endif()

FetchContent_MakeAvailable(JUCE)

# A build for the other architecture must not be installed on this Mac: JUCE's own post-build copy would overwrite the plugins of
# this Mac's architecture in ~/Library/Audio/Plug-Ins, so it is redirected into the build tree, and the install / share steps below are skipped.
if(RL_ARCH STREQUAL _host_arch)
  set(RL_INSTALL ON)
else()
  set(RL_INSTALL OFF)
  set_property(GLOBAL PROPERTY JUCE_VST3_COPY_DIR "${CMAKE_BINARY_DIR}/not-installed/VST3")
  set_property(GLOBAL PROPERTY JUCE_AU_COPY_DIR   "${CMAKE_BINARY_DIR}/not-installed/Components")
  message(STATUS "Building for ${RL_ARCH} on a ${_host_arch} Mac: the bundles are collected in the releases folder only, not installed")
endif()

# All build output lives in ONE place outside iCloud-synced folders: ~/Developer/VIBR
#   - the CMake build tree (configure with: cmake -S . -B ~/Developer/VIBR)
#   - plus, after every build, the signed bundles collected in  <RELEASES_DIR> = <BUILD_OUTPUT_DIR>/releases/{effects,drumcomputer,multieffect}/{VST3,AU,Standalone}
set(BUILD_OUTPUT_DIR "$ENV{HOME}/Developer/VIBR" CACHE PATH "Collects the signed VST3 / AU / Standalone bundles of every plugin")
if(NOT "${CMAKE_BINARY_DIR}" STREQUAL "${BUILD_OUTPUT_DIR}" AND NOT "${CMAKE_BINARY_DIR}" STREQUAL "${BUILD_OUTPUT_DIR}-${RL_ARCH}")
  message(WARNING "Build tree is ${CMAKE_BINARY_DIR}; the project builds in ${BUILD_OUTPUT_DIR} (cmake -S . -B ${BUILD_OUTPUT_DIR}).")
endif()

# The VIBR icon of every plugin bundle (VST3, AU, Standalone): each plugin passes it to juce_add_plugin as ICON_BIG.
# assets/icon.svg is the logo (the same file as website/icon.svg); assets/icon.png is the 1024 x 1024 app icon rendered from it.
set(RL_ICON "${CMAKE_SOURCE_DIR}/assets/icon.png")

# The signed VST3 / AU / Standalone bundles of every plugin are collected here after each build.
# Layout: <RELEASES_DIR>/{effects,drumcomputer,multieffect}/{VST3,AU,Standalone}, one RELEASES_DIR per architecture:
#   ~/Developer/VIBR/releases/arm64  and  ~/Developer/VIBR/releases/x86_64
set(RELEASES_DIR "${BUILD_OUTPUT_DIR}/releases/${RL_ARCH}" CACHE PATH "Folder that collects the signed bundles of this architecture, split in effects / drumcomputer / multieffect (each with VST3, AU and Standalone subfolders)")

# Optional extra folders that receive a copy of every built VST3 (a ;-separated list). Off by default.
set(VST_COPY_DIRS "" CACHE STRING "Extra folders that receive a copy of every built VST3 bundle")

# Signing identity. A self-signed code-signing certificate (Keychain: "RL Plugins Local Signing") gives every rebuild the
# same signature identity, so macOS approvals survive rebuilds. Falls back to ad-hoc ("-") when the identity is missing.
set(SIGN_IDENTITY "RL Plugins Local Signing" CACHE STRING "codesign identity for built bundles ('-' = ad-hoc)")
set(_sign_id "-")
if(APPLE)
  execute_process(COMMAND security find-identity -p codesigning OUTPUT_VARIABLE _ids ERROR_QUIET)
  if(NOT SIGN_IDENTITY STREQUAL "-" AND _ids MATCHES "\"${SIGN_IDENTITY}\"")
    set(_sign_id "${SIGN_IDENTITY}")
  else()
    message(WARNING "Signing identity '${SIGN_IDENTITY}' not found; falling back to ad-hoc signing.")
  endif()
  message(STATUS "Code signing identity: ${_sign_id}")
endif()

# Every built VST3 is also copied (attribute-stripped) to this folder. Empty = off.
#set(SHARED_COPY_DIR "/Users/Shared/RL" CACHE PATH "Folder that receives a clean copy of every built VST3 after each build")

# Call from a plugin's CMakeLists.txt (post-build commands must live in the target's own directory).
# After a build this
#   1. re-signs the VST3 / AU / Standalone bundles (with SIGN_IDENTITY, ad-hoc if missing), so the signature covers the whole bundle - hosts such as
#      Ableton can reject the bare linker signature JUCE leaves behind;
#   2. re-installs the signed VST3 and AU into ~/Library/Audio/Plug-Ins (JUCE's own install step runs before signing);
#   3. collects the signed VST3 / AU / Standalone bundles in RELEASES_DIR/<group>/{VST3,AU,Standalone}, where <group> is the
#      source folder of the plugin: effects (src/effects), drumcomputer (src/drumcomputer) or multieffect (src/multieffect);
#   4. copies the signed VST3 into each folder in VST_COPY_DIRS (none by default).
function(copy_vst3_after_build plugin)
  # releases are split like src/: effects, drumcomputer, multieffect
  set(_group effects)
  if(CMAKE_CURRENT_SOURCE_DIR MATCHES "/src/drumcomputer/")
    set(_group drumcomputer)
  elseif(CMAKE_CURRENT_SOURCE_DIR MATCHES "/src/multieffect/")
    set(_group multieffect)
  endif()
  set(_rel "${RELEASES_DIR}/${_group}")
  if(APPLE)
    foreach(fmt VST3 AU Standalone)
      if(TARGET ${plugin}_${fmt})
        add_custom_command(TARGET ${plugin}_${fmt} POST_BUILD
          # build/ lives under ~/Documents, where iCloud adds xattrs (FinderInfo, fileprovider) that codesign rejects
          COMMAND xattr -cr "$<TARGET_BUNDLE_DIR:${plugin}_${fmt}>"
          COMMAND codesign --force --deep --sign "${_sign_id}" "$<TARGET_BUNDLE_DIR:${plugin}_${fmt}>"
          COMMENT "Signing ${plugin} ${fmt}"
          VERBATIM)
      endif()
    endforeach()

    # collect the signed bundles in RELEASES_DIR/<group>/<format> (runs after the signing step above)
    foreach(fmt VST3 AU Standalone)
      if(TARGET ${plugin}_${fmt})
        add_custom_command(TARGET ${plugin}_${fmt} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E make_directory "${_rel}/${fmt}"
          COMMAND ${CMAKE_COMMAND} -E remove_directory "${_rel}/${fmt}/$<TARGET_BUNDLE_DIR_NAME:${plugin}_${fmt}>"
          COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${plugin}_${fmt}>" "${_rel}/${fmt}/$<TARGET_BUNDLE_DIR_NAME:${plugin}_${fmt}>"
          COMMENT "Collecting ${plugin} ${fmt} in ${_rel}/${fmt}"
          VERBATIM)
      endif()
    endforeach()

    if(TARGET ${plugin}_VST3 AND RL_INSTALL)
      set(vst3_install "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
      add_custom_command(TARGET ${plugin}_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${vst3_install}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${vst3_install}/$<TARGET_BUNDLE_DIR_NAME:${plugin}_VST3>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${plugin}_VST3>" "${vst3_install}/$<TARGET_BUNDLE_DIR_NAME:${plugin}_VST3>"
        COMMENT "Installing signed ${plugin} VST3"
        VERBATIM)
    endif()
    if(TARGET ${plugin}_VST3 AND SHARED_COPY_DIR AND RL_INSTALL)
      add_custom_command(TARGET ${plugin}_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SHARED_COPY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${SHARED_COPY_DIR}/$<TARGET_BUNDLE_DIR_NAME:${plugin}_VST3>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${plugin}_VST3>" "${SHARED_COPY_DIR}/$<TARGET_BUNDLE_DIR_NAME:${plugin}_VST3>"
        COMMAND xattr -cr "${SHARED_COPY_DIR}/$<TARGET_BUNDLE_DIR_NAME:${plugin}_VST3>"
        COMMENT "Copying ${plugin} VST3 to ${SHARED_COPY_DIR}"
        VERBATIM)
    endif()
    if(TARGET ${plugin}_AU AND RL_INSTALL)
      set(au_install "$ENV{HOME}/Library/Audio/Plug-Ins/Components")
      add_custom_command(TARGET ${plugin}_AU POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${au_install}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${au_install}/$<TARGET_BUNDLE_DIR_NAME:${plugin}_AU>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${plugin}_AU>" "${au_install}/$<TARGET_BUNDLE_DIR_NAME:${plugin}_AU>"
        COMMENT "Installing signed ${plugin} AU"
        VERBATIM)
    endif()
  endif()

  if(TARGET ${plugin}_VST3)
    foreach(dir IN LISTS VST_COPY_DIRS)
      add_custom_command(TARGET ${plugin}_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${dir}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${dir}/$<TARGET_BUNDLE_DIR_NAME:${plugin}_VST3>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_BUNDLE_DIR:${plugin}_VST3>"
                "${dir}/$<TARGET_BUNDLE_DIR_NAME:${plugin}_VST3>"
        COMMENT "Copying ${plugin} VST3 to ${dir}"
        VERBATIM)
    endforeach()
  endif()
endfunction()

# ---- sources: everything lives in src/ ----
#   src/common/        header-only code shared by several plugins (target rl_common)
#   src/effects/<name>/ one folder per effect plugin (DD-1, DM-1, ... SD-3); each has its own CMakeLists.txt
#   src/drumcomputer/  the TR-08 drum machine (src/drumcomputer/tr08; an instrument, not a pedal)
add_subdirectory(src/common)

set(EFFECTS dd1 dm1 ce1 re1 re2 ds1 bf3 cs3 lm2 ph2 ge7 eh2 tr2 ma1 ir2 sl2 od2 oc5 bs1 mm1 sd1 sd3 sp1 rl1 mf1 hm2 gr1 sr1 vc1 pt1 vr1)
foreach(effect IN LISTS EFFECTS)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/effects/${effect}/CMakeLists.txt)     # a pedal whose folder was removed is skipped
    add_subdirectory(src/effects/${effect})
  endif()
endforeach()

add_subdirectory(src/multieffect/me1)      # ME-1: self-contained, its own effect modules
add_subdirectory(src/drumcomputer/tr08)

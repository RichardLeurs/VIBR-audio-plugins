#!/usr/bin/env bash
#
# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#
# Builds the plugins, one CPU architecture per build (smaller bundles than a universal binary). After each build the signed
# VST3 / AU / Standalone bundles are collected in
#   ~/Developer/VIBR/releases/<arch>/{new,vintage,experimental,mastering,multieffect,drumcomputer}/{VST3,AU,Standalone}     (<arch> = arm64 or x86_64; on Windows win64 or winarm64, on Linux linux64 or linuxarm64, both without AU)
# 
# After each build every releases/<arch>/<group>/<format> folder is zipped to ~/Developer/VIBR/zip/VIBR-<group>-<OS>-<format>-<arch>-<version>.zip (e.g. VIBR-new-macOS-AU-arm64-0.2.0.zip; <OS> = macOS, Windows or Linux). The group of each plugin is set in config/plugins.conf.
# Linux (linux64 = x86_64): run this script on a Linux machine with a C++20 compiler, CMake and the JUCE dependencies (see INSTALL.md): VST3 and Standalone, no AU.
# Windows (win64 = x64, winarm64 = ARM64): run this script on a Windows machine in Git Bash with Visual Studio 2022 (C++ desktop workload; for ARM64 also the "MSVC ARM64 build tools" component) and CMake installed. A Mac cannot build the Windows plugins.
#   winarm64 can be cross-compiled on an x64 PC (the result runs on Windows on ARM only) or built natively on a Windows ARM PC.
#
#   ./build.sh                    build everything for this ARM architecture (build tree ~/Developer/VIBR)
#   ./build.sh --arch x86_64      build everything for Intel (build tree ~/Developer/VIBR-x86_64)
#   ./build.sh --arch all         arm64 and x86_64 one after the other
#   ./build.sh --arch win64       (on Windows) build everything for 64-bit Windows: VST3 and Standalone in ~/Developer/VIBR/releases/win64 (build tree ~/Developer/VIBR-win64)
#   ./build.sh --arch winarm64    (on Windows) the same for Windows on ARM: releases/winarm64 (build tree ~/Developer/VIBR-winarm64 when cross-compiling on an x64 PC)
#   ./build.sh DD-1 SD-3          build only these plugins (DD-1 CE-1 RE-1 RE-2 DS-1 BF-3 CS-3 LM-2 PH-2 EH-2 TR-2 ME-1 SL-2 OD-2 OC-5 BS-1 SD-3 SU-1 EQ-1 SA-1 ML-1 DL-1 DN-1 DV-1 AL-1 RH-1 RB-1 MS-1 TR-8)
#   ./build.sh --clean [names]    rebuild from scratch (the bundles are collected in releases / test on every build, also when nothing was recompiled)
#   ./build.sh --configure        only (re)configure
#   ./build.sh --no-zip           skip the zip step
#   ./build.sh --install          also install the VST3 / AU in ~/Library/Audio/Plug-Ins (default: only ~/Developer/VIBR/releases; auval needs the install)
#   JOBS=4 ./build.sh             number of parallel jobs (default 8)
#
# While building, three lines are redrawn in place (on a terminal): the progress bar [0-100 %], the current build step, and the warning / error count with the
# last message. At the end: "Build completed" or "Build failed" (with the errors). The full cmake output is in <build tree>/build.log. Without a terminal
# (CI, pipes) it prints a line every 10 % and each warning / error.
set -euo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${VIBR_OUT:-$HOME/Developer/VIBR}"      # VIBR_OUT=<dir> builds in another tree (parallel builds of different plugins)
VERSION="0.2.0"      # the release version: the end of every zip name (keep it the same as project(... VERSION) in config/CMakeLists.txt and VERSION in website/script.js)
HOST="$(uname -m)"
IS_WIN=0 IS_LINUX=0
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) IS_WIN=1; case "$HOST" in aarch64|arm64) HOST="winarm64" ;; *) HOST="win64" ;; esac ;;   Linux*) IS_LINUX=1; [ "$HOST" = x86_64 ] && HOST="linux64" || HOST="linuxarm64" ;; esac        # Git Bash on Windows / Linux (linux64 = x86_64, linuxarm64 = aarch64)
JOBS="${JOBS:-8}"

clean=0 configure_only=0 zip=1 install=OFF arch="$HOST" targets=()
while [ $# -gt 0 ]; do
  case "$1" in
    --clean)     clean=1 ;;
    --configure) configure_only=1 ;;
    --no-zip)    zip=0 ;;
    --install)   install=ON ;;
    --arch)      shift; arch="${1:-}" ;;
    -h|--help)   sed -n '3,25p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*)          echo "unknown option: $1" >&2; exit 2 ;;
    *)           targets+=("${1%_All}_All") ;;      # DD-1 or DD-1_All
  esac
  shift
done
case "$arch" in arm64|x86_64|all|win64|winarm64|linux64|linuxarm64) ;; *) echo "--arch must be arm64, x86_64, all, win64, winarm64, linux64 or linuxarm64" >&2; exit 2 ;; esac
if [[ "$arch" == win* ]] && [ "$IS_WIN" = 0 ]; then echo "--arch $arch must be run on Windows (Git Bash with Visual Studio 2022 and CMake): a Mac cannot build the Windows plugins" >&2; exit 2; fi
if [[ "$arch" != win* ]] && [ "$IS_WIN" = 1 ]; then echo "on Windows only --arch win64 or winarm64 works" >&2; exit 2; fi
if [ "$IS_LINUX" = 1 ] && [ "$arch" != "$HOST" ]; then echo "on Linux only --arch $HOST works" >&2; exit 2; fi
if [ "$IS_LINUX" = 0 ] && [[ "$arch" == linux* ]]; then echo "--arch $arch must be run on Linux" >&2; exit 2; fi

# ---- progress display -------------------------------------------------------------------------------------------------------------------------------------
# run_build <tree> <log> <cmake --build args...>: runs the build, shows  [####------] 42 %  /  the current step  /  warnings, errors and the last message
# (three lines redrawn in place on a terminal), writes everything to <log> and returns cmake's exit status.
BAR_W=30
BAR_FULL="$(printf '%*s' "$BAR_W" '' | tr ' ' '#')"; BAR_EMPTY="$(printf '%*s' "$BAR_W" '' | tr ' ' '-')"
if [ -t 1 ]; then TTY=1; else TTY=0; fi
if [ "$TTY" = 1 ]; then C_GREEN=$'\033[32m'; C_YELLOW=$'\033[33m'; C_RED=$'\033[31m'; C_DIM=$'\033[2m'; C_OFF=$'\033[0m'; else C_GREEN= C_YELLOW= C_RED= C_DIM= C_OFF=; fi

run_build() {
  local tree="$1" log="$2"; shift 2
  local rc=0
  : > "$log"
  set +e
  cmake "$@" 2>&1 | render_build "$log"
  rc=${PIPESTATUS[0]}
  set -e
  return "$rc"
}

render_build() {
  local log="$1" last="" line rest step="starting ..." pct=0 shown=-1 lastpct=-10 warn=0 err=0 msg="" kind cols n m
  cols="$(tput cols 2>/dev/null || echo 100)"; [ "$cols" -ge 40 ] || cols=100
  local w=$((cols - 1))
  [ "$TTY" = 1 ] && printf '\n\n\n'
  draw() {
    [ "$TTY" = 1 ] || return 0
    local f=$((pct * BAR_W / 100)) l1 l2 l3 col="$C_DIM"
    l1="Progress [${BAR_FULL:0:f}${BAR_EMPTY:0:BAR_W-f}] $(printf '%3d' "$pct") %"
    l2="Step     ${step}"
    if [ "$err" -gt 0 ]; then col="$C_RED"; elif [ "$warn" -gt 0 ]; then col="$C_YELLOW"; fi
    if [ "$warn" -eq 0 ] && [ "$err" -eq 0 ]; then l3="Messages none"; else l3="Messages ${warn} warning(s), ${err} error(s): ${msg}"; fi
    [ "$l1|$l2|$l3" = "$last" ] && return 0                                  # nothing changed: no redraw
    last="$l1|$l2|$l3"
    printf '\033[3A\r\033[2K%s\n\r\033[2K%s\n\r\033[2K%s%s%s\n' "${l1:0:w}" "${l2:0:w}" "$col" "${l3:0:w}" "$C_OFF"
  }
  while IFS= read -r line; do
    printf '%s\n' "$line" >> "$log"
    rest="$line"
    if [[ "$line" =~ ^\[\ *([0-9]+)%\]\ (.*)$ ]]; then pct="${BASH_REMATCH[1]}"; rest="${BASH_REMATCH[2]}"                      # Makefiles: [ 42%] Building CXX object ...
    elif [[ "$line" =~ ^\[([0-9]+)/([0-9]+)\]\ (.*)$ ]]; then n="${BASH_REMATCH[1]}"; m="${BASH_REMATCH[2]}"; rest="${BASH_REMATCH[3]}"; [ "$m" -gt 0 ] && pct=$((n * 100 / m))      # Ninja: [12/340] ...
    fi
    [ "$pct" -gt 100 ] && pct=100
    line="${line//\"/}"; rest="${rest//\"/}"
    kind=""
    if [[ "$line" == *"error:"* || "$line" == *"fatal error"* || "$line" == *"*** "* || "$line" == *"Error "[0-9]* ]]; then kind=error; err=$((err + 1))
    elif [[ "$line" == *"warning:"* ]]; then kind=warning; warn=$((warn + 1)); fi
    if [ -n "$kind" ]; then
      msg="$(printf '%s' "$line" | sed -E -e 's#^.*/(src|tools|config)/#\1/#' -e 's/  +/ /g')"
      [ "$TTY" = 0 ] && printf '%s: %s\n' "$kind" "$msg"
    elif [[ "$rest" =~ ^Building\ [A-Za-z]+\ object\ (.*)\.o$ ]]; then step="Compiling $(basename "${BASH_REMATCH[1]}")"
    elif [[ "$rest" =~ ^Linking\ [A-Za-z]+\ (.*)$ ]]; then step="Linking $(basename "${BASH_REMATCH[1]}")"
    elif [[ "$rest" =~ ^Built\ target\ (.*)$ ]]; then step="Finished target ${BASH_REMATCH[1]}"
    elif [[ "$rest" =~ ^(Copying|Generating|Running|Creating|Signing|Packaging)\ (.*)$ ]]; then step="${BASH_REMATCH[1]} $(basename "${BASH_REMATCH[2]%% *}")"
    elif [[ "$rest" == Scanning* || "$rest" == Consolidate* ]]; then step="Preparing the build"
    fi
    if [ "$TTY" = 1 ]; then draw
    elif [ $((pct - lastpct)) -ge 10 ]; then lastpct=$pct; printf '[%3d%%] %s\n' "$pct" "$step"; fi
  done
  draw
}

build_arch() {
  local a="$1" tree="$OUT" ok=1
  [ "$a" = "$HOST" ] || tree="$OUT-$a"            # this Mac's own architecture keeps the original build tree
  mkdir -p "$tree"
  if [ -f "$tree/CMakeCache.txt" ] && ! grep -qx "CMAKE_HOME_DIRECTORY:INTERNAL=$SRC/config" "$tree/CMakeCache.txt"; then rm -f "$tree/CMakeCache.txt"; fi      # the tree was configured from another source folder (the repository root): configure again
  echo "==> configure  ($a: $SRC -> $tree)"
  local gen=()
  if [ "$IS_WIN" = 1 ] && [ -z "${CMAKE_GENERATOR:-}" ]; then case "$a" in winarm64) gen=(-A ARM64) ;; *) gen=(-A x64) ;; esac; fi      # no -G: CMake picks the newest Visual Studio it finds (2022, 2026 ...); set CMAKE_GENERATOR to use another (e.g. Ninja)
  if ! cmake -S "$SRC/config" -B "$tree" ${gen[@]+"${gen[@]}"} -DRL_ARCH="$a" -DRL_INSTALL_PLUGINS="$install" -DRELEASES_DIR="$OUT/releases/$a" > "$tree/configure.log" 2>&1; then
    echo "${C_RED}Configure failed${C_OFF} (log: $tree/configure.log):"; grep -E "CMake Error|error" "$tree/configure.log" | head -20; exit 1
  fi
  [ "$configure_only" = 1 ] && return 0

  local args=(--build "$tree" --config Release -j "$JOBS")
  [ "$clean" = 1 ] && args+=(--clean-first)
  if [ "${#targets[@]}" -gt 0 ]; then
    args+=(--target "${targets[@]}")
    echo "==> build      $a: ${targets[*]}"
  else
    echo "==> build      $a: all plugins"
  fi
  local log="$tree/build.log" t0=$SECONDS
  if run_build "$tree" "$log" "${args[@]}"; then ok=1; else ok=0; fi
  local nw ne; nw="$(grep -c 'warning:' "$log" || true)"; ne="$(grep -cE 'error:|\*\*\* ' "$log" || true)"
  if [ "$ok" = 1 ]; then
    echo "${C_GREEN}Build completed${C_OFF} ($a, $((SECONDS - t0)) s, $nw warning(s), $ne error(s); log: $log)"
  else
    echo "${C_RED}Build failed${C_OFF} ($a, $((SECONDS - t0)) s, $nw warning(s), $ne error(s); log: $log)"
    grep -E 'error:|fatal error|\*\*\* ' "$log" | sed -E 's#^.*/(src|tools|config)/#\1/#' | head -20
    exit 1
  fi
  if [ "$zip" = 1 ]; then
    mkdir -p "$OUT/zip"
    local g f d z OSNAME=macOS
    [ "$IS_WIN" = 1 ] && OSNAME=Windows; [ "$IS_LINUX" = 1 ] && OSNAME=Linux      # the OS name in the zip names
    for g in new vintage experimental mastering multieffect drumcomputer; do
      for f in VST3 AU Standalone; do                # (win64 has no AU folder: it is skipped)
        d="$OUT/releases/$a/$g/$f"; z="$OUT/zip/VIBR-$g-$OSNAME-$f-$a-$VERSION.zip"
        [ -d "$d" ] || continue
        cp "$SRC/INSTALL.md" "$d/INSTALL.md"; cp "$SRC/LICENSE" "$d/LICENSE"      # every zip holds the install notes and the license (INSTALL.md links to LICENSE)
        rm -f "$z"
        if [ "$IS_WIN" = 1 ] || [ "$IS_LINUX" = 1 ]; then (cd "$OUT/releases/$a/$g" && cmake -E tar cf "$z" --format=zip "$f")      # the zip holds the folder VST3 / Standalone, as on the Mac (Windows and Linux)
        else ditto -c -k --keepParent "$d" "$z"; fi          # ditto keeps the code signature and symlinks of the bundles intact
      done
    done
    echo "==> zip        $a in $OUT/zip"
  fi
  echo "==> done       $a bundles in $OUT/releases/$a/{new,vintage,experimental,mastering,multieffect,drumcomputer}/{VST3,AU,Standalone}"
}

print_banner() {
  printf '%s' "$C_GREEN"
  cat <<'ART'

__     _____ ____  ____
\ \   / /_ _| __ )|  _ \
 \ \ / / | ||  _ \| |_) |
  \ V /  | || |_) |  _ <
   \_/  |___|____/|_| \_\
   
ART
  printf '%s\n VIBR Audio Plugins | Copyright (c) Richard Leurs 2026 | GPL3.0\n\n' "$C_OFF"
}

print_banner
if [ "$arch" = all ]; then build_arch arm64; build_arch x86_64; else build_arch "$arch"; fi

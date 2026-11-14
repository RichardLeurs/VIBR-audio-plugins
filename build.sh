#!/usr/bin/env bash
#
# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#
# Builds the plugins, one CPU architecture per build (smaller bundles than a universal binary). After each build the signed
# VST3 / AU / Standalone bundles are collected in
#   ~/Developer/VIBR/releases/<arch>/{new,vintage,experimental,mastering,multieffect,drumcomputer}/{VST3,AU,Standalone}     (<arch> = arm64 or x86_64; on Windows win64, without AU)
# 
# After each build every releases/<arch>/<group>/<format> folder is zipped to ~/Developer/VIBR/zip/<group>-<format>-<arch>.zip (e.g. new-AU-arm64.zip). The group of each plugin is set in plugins.conf. for sending to another computer.
# Windows (win64): run this script on a Windows machine in Git Bash with Visual Studio 2022 (C++ desktop workload) and CMake installed. A Mac cannot build the Windows plugins.
#
#   ./build.sh                    build everything for this Mac's architecture (build tree ~/Developer/VIBR)
#   ./build.sh --arch x86_64      build everything for Intel (build tree ~/Developer/VIBR-x86_64)
#   ./build.sh --arch all         arm64 and x86_64 one after the other
#   ./build.sh --arch win64       (on Windows) build everything for 64-bit Windows: VST3 and Standalone in ~/Developer/VIBR/releases/win64 (build tree ~/Developer/VIBR-win64)
#   ./build.sh DD-1 SD-3          build only these plugins (DD-1 DM-1 CE-1 RE-1 RE-2 DS-1 BF-3 CS-3 LM-2 PH-2 GE-7 EH-2 TR-2 IR-2 ME-1 SL-2 OD-2 OC-5 BS-1 MM-1 SD-1 SD-3 SP-1 RV-1 MF-1 HM-2 GR-1 SR-1 VC-1 PT-1 VR-1 DY-1 SA-1 SU-1 ML-1 DL-1 DN-1 DV-1 RH-1 RB-1 MS-1 TR-8)
#   ./build.sh --clean [names]    rebuild from scratch (needed to re-run the collect / copy steps when nothing changed)
#   ./build.sh --configure        only (re)configure
#   ./build.sh --no-zip           skip the zip step
#   JOBS=4 ./build.sh             number of parallel jobs (default 8)
set -euo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$HOME/Developer/VIBR"
HOST="$(uname -m)"
IS_WIN=0
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) IS_WIN=1; HOST="win64" ;; esac        # Git Bash on Windows
JOBS="${JOBS:-8}"

clean=0 configure_only=0 zip=1 arch="$HOST" targets=()
while [ $# -gt 0 ]; do
  case "$1" in
    --clean)     clean=1 ;;
    --configure) configure_only=1 ;;
    --no-zip)    zip=0 ;;
    --arch)      shift; arch="${1:-}" ;;
    -h|--help)   sed -n '3,18p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*)          echo "unknown option: $1" >&2; exit 2 ;;
    *)           targets+=("${1%_All}_All") ;;      # DD-1 or DD-1_All
  esac
  shift
done
case "$arch" in arm64|x86_64|all|win64) ;; *) echo "--arch must be arm64, x86_64, all or win64" >&2; exit 2 ;; esac
if [ "$arch" = win64 ] && [ "$IS_WIN" = 0 ]; then echo "--arch win64 must be run on Windows (Git Bash with Visual Studio 2022 and CMake): a Mac cannot build the Windows plugins" >&2; exit 2; fi
if [ "$arch" != win64 ] && [ "$IS_WIN" = 1 ]; then echo "on Windows only --arch win64 works" >&2; exit 2; fi

build_arch() {
  local a="$1" tree="$OUT"
  [ "$a" = "$HOST" ] || tree="$OUT-$a"            # this Mac's own architecture keeps the original build tree
  mkdir -p "$tree"
  echo "==> configure  ($a: $SRC -> $tree)"
  local gen=()
  if [ "$IS_WIN" = 1 ] && [ -z "${CMAKE_GENERATOR:-}" ]; then gen=(-G "Visual Studio 17 2022" -A x64); fi      # set CMAKE_GENERATOR to use another (e.g. Ninja)
  cmake -S "$SRC" -B "$tree" ${gen[@]+"${gen[@]}"} -DRL_ARCH="$a" -DRELEASES_DIR="$OUT/releases/$a"
  [ "$configure_only" = 1 ] && return 0

  local args=(--build "$tree" --config Release -j "$JOBS")
  [ "$clean" = 1 ] && args+=(--clean-first)
  if [ "${#targets[@]}" -gt 0 ]; then
    args+=(--target "${targets[@]}")
    echo "==> build      $a: ${targets[*]}"
  else
    echo "==> build      $a: all plugins"
  fi
  cmake "${args[@]}"
  if [ "$zip" = 1 ]; then
    mkdir -p "$OUT/zip"
    local g f d z
    for g in new vintage experimental mastering multieffect drumcomputer; do
      for f in VST3 AU Standalone; do                # (win64 has no AU folder: it is skipped)
        d="$OUT/releases/$a/$g/$f"; z="$OUT/zip/$g-$f-$a.zip"
        [ -d "$d" ] || continue
        rm -f "$z"
        if [ "$IS_WIN" = 1 ]; then (cd "$OUT/releases/$a/$g" && cmake -E tar cf "$z" --format=zip "$f")      # the zip holds the folder VST3 / Standalone, as on the Mac
        else ditto -c -k --keepParent "$d" "$z"; fi          # ditto keeps the code signature and symlinks of the bundles intact
      done
    done
    echo "==> zip        $a in $OUT/zip"
  fi
  echo "==> done       $a bundles in $OUT/releases/$a/{new,vintage,experimental,mastering,multieffect,drumcomputer}/{VST3,AU,Standalone}"
}

if [ "$arch" = all ]; then build_arch arm64; build_arch x86_64; else build_arch "$arch"; fi

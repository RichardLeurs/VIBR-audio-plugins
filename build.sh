#!/usr/bin/env bash
# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# Builds the plugins into ~/Developer/Plugins-build (outside iCloud). After each build the signed VST3 / AU / Standalone
# bundles are collected in ~/Developer/Plugins-build/releases/{VST3,AU,Standalone}, installed in ~/Library/Audio/Plug-Ins and copied
# to /Users/Shared/RL (see CMakeLists.txt).
#
#   ./build.sh                    build everything
#   ./build.sh DD-1 SD-3          build only these plugins (DD-1 DM-1 CE-1 RE-1 RE-2 DS-1 BF-3 CS-3 LM-2 PH-2 GE-7 EH-2 IR-2 DC-1 SL-2 OD-2 OC-5 BS-1 MM-1 SD-1 SD-3 TR-8)
#   ./build.sh --clean [names]    rebuild from scratch (needed to re-run the collect / copy steps when nothing changed)
#   ./build.sh --configure        only (re)configure
#   JOBS=4 ./build.sh             number of parallel jobs (default 8)
set -euo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$HOME/Developer/Plugins-build"
JOBS="${JOBS:-8}"

clean=0 configure_only=0 targets=()
for arg in "$@"; do
  case "$arg" in
    --clean)     clean=1 ;;
    --configure) configure_only=1 ;;
    -h|--help)   sed -n '3,11p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*)          echo "unknown option: $arg" >&2; exit 2 ;;
    *)           targets+=("${arg%_All}_All") ;;      # DD-1 or DD-1_All
  esac
done

mkdir -p "$BUILD"
echo "==> configure  ($SRC -> $BUILD)"
cmake -S "$SRC" -B "$BUILD"
[ "$configure_only" = 1 ] && exit 0

args=(--build "$BUILD" --config Release -j "$JOBS")
[ "$clean" = 1 ] && args+=(--clean-first)
if [ "${#targets[@]}" -gt 0 ]; then
  args+=(--target "${targets[@]}")
  echo "==> build      ${targets[*]}"
else
  echo "==> build      all plugins"
fi
cmake "${args[@]}"

echo "==> done       bundles in $BUILD/releases/{VST3,AU,Standalone}"

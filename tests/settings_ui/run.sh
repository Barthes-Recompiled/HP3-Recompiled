#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export PATH=/c/msys64/ucrt64/bin:$PATH
build_dir="$PWD/port/build-win/settings-ui-check"
mkdir -p port/build-win/settings-ui-check
export TMP="$(cygpath -m "$build_dir")"
export TEMP="$TMP" TMPDIR="$TMP"
g++ -std=c++20 -O0 -DWIN32_LEAN_AND_MEAN -DNOMINMAX \
    -Iport/src/scda -Iport/third_party/imgui \
    tests/settings_ui/check_ui.cpp port/src/scda/app/rows.cpp \
    port/third_party/imgui/imgui.cpp port/third_party/imgui/imgui_draw.cpp \
    port/third_party/imgui/imgui_widgets.cpp port/third_party/imgui/imgui_tables.cpp \
    -luser32 -o "$build_dir/check_ui.exe"
"$build_dir/check_ui.exe"

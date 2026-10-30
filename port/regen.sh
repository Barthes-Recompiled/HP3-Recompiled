#!/usr/bin/env bash
# Disassemble + recompile default.xbe into src/recomp/gen (run from anywhere). Every hand-written override file is
# listed with --exclude-manual so its sub_X keeps the generated body as sub_X_gen.
set -e
cd "$(dirname "$0")/.."
# The hand-written code is in the canonical XBE's addresses (port/region/). For another supported XBE, retarget.py
# writes copies in its addresses to port/src/recomp/region/ and they are used below and by CMake; for the
# canonical one it removes that folder; for an unknown one it stops and lists the supported SHA-1s.
python3 tools/region/retarget.py --port port --xbe game/default.xbe --out port/src/recomp/region
src() { if [ -f "port/src/recomp/region/$1" ]; then echo "port/src/recomp/region/$1"; else echo "port/$1"; fi; }
# the XBE's layout (sections, imports, entry), which disasm reads as game/default_analysis.json
python3 -m tools.xbe_parser game/default.xbe --json game/default_analysis.json
python3 -m tools.disasm game/default.xbe --text-only --extra-sections D3D,D3DX,XGRPH,DSOUND,XMV,XPP \
    --seed-functions "$(src icall_seeds.json)"
python3 -m tools.func_id game/default.xbe
python3 -m tools.abi_analysis game/default.xbe
python3 -m tools.recomp game/default.xbe --all --split 1000 --gen-dir port/src/recomp/gen --game-name hppoa \
    --exclude-manual "$(src src/hle_dsound.c)" \
    --exclude-manual "$(src src/hle_video.c)" \
    --exclude-manual "$(src src/hle_input.c)" \
    --exclude-manual "$(src src/hle_xapi.c)" \
    --exclude-manual "$(src src/hle_trainer.c)" \
    --exclude-manual "$(src src/hle_skip.c)" \
    --exclude-manual "$(src src/scda/xdk/hle_d3d8.cpp)"
# hooks inside generated functions (debug traces) that an override file can't express
python3 "$(src tools/patch_gen.py)" port/src/recomp/gen

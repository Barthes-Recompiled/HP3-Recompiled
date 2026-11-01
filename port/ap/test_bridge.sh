#!/usr/bin/env bash
# Build the bridge host test (test_bridge.cpp + src/ap) twice, in MSYS2 UCRT64: with port/ap/catalog.json and with
# a copy whose version is bumped (the fence build). Run them against a local Archipelago server hosting a seed made
# from apworld/hppoa (see docs/ap/10-implementation-log.md, Phase 2).
#   ap/test_bridge.sh [out dir]        (default build-ap-test)
set -euo pipefail
cd "$(dirname "$0")/.."
out=${1:-build-ap-test}
mkdir -p "$out/gen" "$out/gen-fence"
cmake -DIN=ap/catalog.json -DOUT="$out/gen/catalog_data.inc" -P ap/embed.cmake
python -c "import json,sys; c=json.load(open('ap/catalog.json',encoding='utf-8')); c['version']='000000000000'; \
open(sys.argv[1],'w',encoding='utf-8',newline='\n').write(json.dumps(c,indent=1,ensure_ascii=False)+'\n')" \
    "$out/catalog-fence.json"
cmake -DIN="$out/catalog-fence.json" -DOUT="$out/gen-fence/catalog_data.inc" -P ap/embed.cmake
for build in gen gen-fence; do
    exe=$out/test_bridge${build#gen}.exe
    g++ -std=c++20 -O1 -Ithird_party -Isrc/ap -I"$out/$build" ap/test_bridge.cpp src/ap/bridge.cpp src/ap/client.cpp \
        -o "$exe" -lwinhttp -static
    echo "built $exe"
done

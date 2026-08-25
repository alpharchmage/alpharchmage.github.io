#!/usr/bin/env bash

# 单文件核心：网页只从 cpp/name_arena.cpp 构建 C++/WASM 战斗逻辑。
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/client/src/wasm"

mkdir -p "$OUT"

em++ -std=c++17 -O3 --no-entry \
    "$ROOT/cpp/name_arena.cpp" \
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=83886080 \
    -sEXPORTED_FUNCTIONS='["_name_arena_player_snapshot","_name_arena_simulate_battle"]' \
    -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    -o "$OUT/name_arena_core.mjs"

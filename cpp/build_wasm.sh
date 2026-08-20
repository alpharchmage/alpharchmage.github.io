#!/usr/bin/env bash

# 任何命令失败、未定义变量或管道失败都会立即退出，避免留下半更新的 WASM 文件。
set -euo pipefail

# ROOT_DIR 是项目根目录；脚本本身位于 <root>/cpp/build_wasm.sh。
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
# 产物进入 React 源码树，Vite 会把 .mjs 和 .wasm 加入最终网页构建图。
OUT_DIR="$ROOT_DIR/client/src/wasm"

mkdir -p "$OUT_DIR"

# 用 Emscripten 的 em++ 编译所有当前运行必需的 C++ 源文件。
# 新增战斗模块时，需要把新的 .cpp 文件加入此列表；否则网页不会运行新逻辑。
# MODULARIZE/EXPORT_ES6 生成可被 TypeScript import 的模块工厂；
# ALLOW_MEMORY_GROWTH 允许输入较长名字列表时扩展 WASM 线性内存。
# EXPORTED_FUNCTIONS/EXPORTED_RUNTIME_METHODS 是保留给网页的符号白名单。
# Emscripten 将同时输出 name_arena_core.mjs 加载器与 name_arena_core.wasm 二进制。
em++ -std=c++17 -O3 --no-entry \
    "$ROOT_DIR/cpp/src/transport.cpp" \
    "$ROOT_DIR/cpp/src/name_identity.cpp" \
    "$ROOT_DIR/cpp/src/attribute_derivation.cpp" \
    "$ROOT_DIR/cpp/src/battle_engine.cpp" \
    "$ROOT_DIR/cpp/src/battle_resolution.cpp" \
    "$ROOT_DIR/cpp/src/battle_simulation.cpp" \
    "$ROOT_DIR/cpp/src/player_snapshot.cpp" \
    "$ROOT_DIR/cpp/src/render_command.cpp" \
    "$ROOT_DIR/cpp/src/wasm_api.cpp" \
    -I"$ROOT_DIR/cpp/include" \
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sALLOW_MEMORY_GROWTH=1 \
    -sEXPORTED_FUNCTIONS='["_name_arena_player_snapshot","_name_arena_simulate_battle"]' \
    -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    -o "$OUT_DIR/name_arena_core.mjs"

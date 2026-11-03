#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
build_dir="$repo_root/build/wasm"

if ! command -v emcmake >/dev/null 2>&1; then
    printf '%s\n' \
        '[ERROR] emcmake was not found. Activate the Emscripten SDK environment first.' >&2
    exit 1
fi

printf '[INFO] Configuring the WebAssembly build in [%s].\n' "$build_dir"
emcmake cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DMOKU_BUILD_WASM=ON \
    -DMOKU_BUILD_ENGINE=OFF \
    -DMOKU_BUILD_TESTS=OFF \
    -DMOKU_BUILD_PYTHON=OFF \
    -DMOKU_BUILD_NNUE=OFF

printf '[INFO] Building the extended WebAssembly engine.\n'
cmake --build "$build_dir" --config Release --target extended_moku_wasm

printf '[INFO] WebAssembly build completed successfully in [%s].\n' "$build_dir"

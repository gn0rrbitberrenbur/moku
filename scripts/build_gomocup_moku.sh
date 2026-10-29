#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
build_dir="$repo_root/build"

printf '%s[INFO] Configuring build for target gomocup_moku in [%s].%s\n' "$green" "$build_dir" "$reset"
cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DMOKU_BUILD_ENGINE=ON \
    -DMOKU_BUILD_TESTS=OFF \
    -DMOKU_BUILD_PYTHON=OFF \
    -DMOKU_BUILD_NNUE=OFF

printf '%s[INFO] Building target gomocup_moku.%s\n' "$green" "$reset"
cmake --build "$build_dir" --config Release --target gomocup_moku
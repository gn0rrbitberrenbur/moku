#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
build_dir="$repo_root/build"

echo "[INFO] Configuring build for target moku_cli in [$build_dir]."
cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DMOKU_BUILD_ENGINE=OFF \
    -DMOKU_BUILD_TESTS=OFF \
    -DMOKU_BUILD_PYTHON=OFF \
    -DMOKU_BUILD_NNUE=OFF \
    -DMOKU_BUILD_CLI=ON

echo "[INFO] Building target moku_cli."
cmake --build "$build_dir" --config Release --target moku_cli
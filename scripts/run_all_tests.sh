#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
build_dir="$repo_root/build"

if [[ ! -d "$build_dir" ]]; then
    echo "[ERROR] Build directory not found: $build_dir" >&2
    exit 1
fi

echo "[INFO] Running all configured tests from [$build_dir]."
ctest --test-dir "$build_dir" --output-on-failure
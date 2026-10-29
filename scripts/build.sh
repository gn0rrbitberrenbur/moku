#!/usr/bin/env bash
set -euo pipefail

red=''
green=''
reset=''

if [[ -t 1 && -z "${NO_COLOR+x}" ]]; then
    green=$'\033[32m'
fi

if [[ -t 2 && -z "${NO_COLOR+x}" ]]; then
    red=$'\033[31m'
fi

if [[ -n "$red$green" ]]; then
    reset=$'\033[0m'
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
build_dir="$repo_root/build"

printf '%s[INFO] Running full CMake configuration in build directory [%s].%s\n' "$green" "$build_dir" "$reset"

if cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DMOKU_BUILD_ENGINE=ON \
    -DMOKU_BUILD_TESTS=ON \
    -DMOKU_BUILD_PYTHON=ON \
    -DMOKU_BUILD_NNUE=ON; then
    :
else
    status=$?
    printf '%s[ERROR] CMake configuration failed in [%s] (exit code %s).%s\n' "$red" "$build_dir" "$status" "$reset" >&2
    exit "$status"
fi

printf '%s[INFO] CMake configuration was successful.%s\n' "$green" "$reset"
printf '%s[INFO] Starting full build process.%s\n' "$green" "$reset"

if cmake --build "$build_dir" --config Release; then
    :
else
    status=$?
    printf '%s[ERROR] Full build failed in [%s] (exit code %s).%s\n' "$red" "$build_dir" "$status" "$reset" >&2
    exit "$status"
fi

printf '%s[INFO] Full build completed successfully in [%s].%s\n' "$green" "$build_dir" "$reset"
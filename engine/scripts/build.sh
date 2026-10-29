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
source_dir="$repo_root/moku"
build_dir="$source_dir/build"

printf '%s[INFO] Running CMake configuration in build directory [%s].%s\n' "$green" "$build_dir" "$reset"

if cmake -S "$source_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release; then
    :
else
    status=$?
    printf '%s[ERROR] CMake configuration failed in [%s] (exit code %s).%s\n' "$red" "$build_dir" "$status" "$reset" >&2
    exit "$status"
fi

printf '%s[INFO] CMake configuration was successful.%s\n' "$green" "$reset"
printf '%s[INFO] Starting build process.%s\n' "$green" "$reset"

if cmake --build "$build_dir" --config Release; then
    :
else
    status=$?
    printf '%s[ERROR] Build failed in [%s] (exit code %s).%s\n' "$red" "$build_dir" "$status" "$reset" >&2
    exit "$status"
fi

printf '%s[INFO] Build completed successfully in [%s].%s\n' "$green" "$build_dir" "$reset"
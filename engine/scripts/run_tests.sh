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

printf '%s[INFO] Running tests in build directory [%s].%s\n' "$green" "$build_dir" "$reset"

pushd "$build_dir" >/dev/null
trap 'popd >/dev/null' EXIT

if ctest -C Release --output-on-failure; then
    printf '%s[INFO] Tests completed successfully in [%s].%s\n' "$green" "$build_dir" "$reset"
else
    status=$?
    printf '%s[ERROR] Tests failed in [%s] (exit code %s).%s\n' "$red" "$build_dir" "$status" "$reset" >&2
    exit "$status"
fi
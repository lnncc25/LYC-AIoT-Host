#!/usr/bin/env bash

set -euo pipefail

readonly project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
readonly public_dir="${project_root}/src/testcases"

mapfile -t public_files < <(
    find "${public_dir}" -maxdepth 1 -type f \
        \( -name '*.h' -o -name '*.cpp' \) -print | sort
)

if [[ ${#public_files[@]} -eq 0 ]]; then
    echo "No public testcase files found" >&2
    exit 1
fi

if rg -n \
    -e 'Case8[0-9]+' \
    -e 'case8[0-9]+/' \
    -e 'presentCase8[0-9]+' \
    "${public_files[@]}"; then
    echo "Public testcase interfaces must not reference private Case8x models" >&2
    exit 1
fi

echo "Testcase public boundary checks passed"

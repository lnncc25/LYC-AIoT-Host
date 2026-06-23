#!/usr/bin/env bash

set -euo pipefail

readonly project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly qmake_bin="${QMAKE_BIN:-/opt/Qt5.12.9/5.12.9/gcc_64/bin/qmake}"
readonly build_dir="${1:-/tmp/aiot-host-qt512-tests}"

if [[ ! -x "${qmake_bin}" ]]; then
    echo "Qt 5.12.9 qmake not found: ${qmake_bin}" >&2
    exit 1
fi

mkdir -p "${build_dir}"
"${qmake_bin}" "${project_root}/tests/tests.pro" -o "${build_dir}/Makefile"
make -C "${build_dir}" -j"${BUILD_JOBS:-2}"
make -C "${build_dir}" check

echo "All Qt tests passed"

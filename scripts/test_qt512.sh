#!/usr/bin/env bash

set -euo pipefail

readonly project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly qmake_bin="${QMAKE_BIN:-/opt/Qt5.12.9/5.12.9/gcc_64/bin/qmake}"
source "${project_root}/scripts/build_dir_guard.sh"

clean_build=false
build_dir="/tmp/aiot-host-qt512-tests"

if [[ "${1:-}" == "--clean" ]]; then
    clean_build=true
    shift
fi

if [[ $# -gt 1 ]]; then
    echo "Usage: $0 [--clean] [build-directory]" >&2
    exit 2
fi

if [[ $# -eq 1 ]]; then
    build_dir="$1"
fi

if [[ ! -x "${qmake_bin}" ]]; then
    echo "Qt 5.12.9 qmake not found: ${qmake_bin}" >&2
    exit 1
fi

build_dir="$(realpath -m "${build_dir}")"
"${project_root}/tests/scripts/test_build_directory_guard.sh"
"${project_root}/tests/scripts/test_testcase_public_boundary.sh"
aiot_prepare_build_directory "${project_root}" "${build_dir}" "tests" "${clean_build}"
"${qmake_bin}" "${project_root}/tests/tests.pro" -o "${build_dir}/Makefile"
make -C "${build_dir}" -j"${BUILD_JOBS:-2}"
make -C "${build_dir}" check

echo "All Qt tests passed"

#!/usr/bin/env bash

set -euo pipefail

readonly project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly qmake_bin="${QMAKE_BIN:-/opt/Qt5.12.9/5.12.9/gcc_64/bin/qmake}"
source "${project_root}/scripts/build_dir_guard.sh"

clean_build=false
build_dir="/tmp/aiot-host-qt512-build"

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
aiot_prepare_build_directory "${project_root}" "${build_dir}" "product" "${clean_build}"
"${qmake_bin}" "${project_root}/AIoT_Test.pro" -o "${build_dir}/Makefile"
make -C "${build_dir}" -j"${BUILD_JOBS:-2}"

echo "Built ${build_dir}/AIoT_Test"

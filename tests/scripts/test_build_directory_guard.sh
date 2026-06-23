#!/usr/bin/env bash

set -euo pipefail

readonly project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${project_root}/scripts/build_dir_guard.sh"

readonly test_root="$(mktemp -d /tmp/aiot-build-guard-test.XXXXXX)"
trap 'rm -rf -- "${test_root}"' EXIT

expect_failure()
{
    local description="$1"
    shift

    if "$@" >/dev/null 2>&1; then
        echo "Expected failure: ${description}" >&2
        exit 1
    fi
}

expect_preserved()
{
    local path="$1"

    if [[ ! -e "${path}" ]]; then
        echo "Expected path to remain: ${path}" >&2
        exit 1
    fi
}

expect_failure "project root" \
    aiot_prepare_build_directory "${project_root}" "${project_root}" "product" true
expect_failure "project source directory" \
    aiot_prepare_build_directory "${project_root}" "${project_root}/src" "product" true
expect_failure "project git directory" \
    aiot_prepare_build_directory "${project_root}" "${project_root}/.git" "product" true
expect_failure "project parent directory" \
    aiot_prepare_build_directory "${project_root}" "$(dirname "${project_root}")" "product" true
expect_failure "filesystem root" \
    aiot_prepare_build_directory "${project_root}" "/" "product" true

expect_failure "build script project source directory" \
    "${project_root}/scripts/build_qt512.sh" --clean "${project_root}/src"
expect_failure "build script project git directory" \
    "${project_root}/scripts/build_qt512.sh" --clean "${project_root}/.git"
expect_failure "build script project parent directory" \
    "${project_root}/scripts/build_qt512.sh" --clean "$(dirname "${project_root}")"

unmarked_dir="${test_root}/unmarked"
mkdir -p "${unmarked_dir}"
touch "${unmarked_dir}/keep"
expect_failure "unmarked directory" \
    aiot_prepare_build_directory "${project_root}" "${unmarked_dir}" "product" true
expect_preserved "${unmarked_dir}/keep"

forged_dir="${test_root}/forged"
mkdir -p "${forged_dir}"
touch "${forged_dir}/keep"
printf 'project_root=%s\nbuild_kind=%s\n' "/wrong/project" "product" \
    > "${forged_dir}/${AIOT_BUILD_MARKER}"
expect_failure "mismatched marker" \
    aiot_prepare_build_directory "${project_root}" "${forged_dir}" "product" true
expect_preserved "${forged_dir}/keep"

wrong_kind_dir="${test_root}/wrong-kind"
mkdir -p "${wrong_kind_dir}"
touch "${wrong_kind_dir}/keep"
aiot_marker_content "${project_root}" "tests" > "${wrong_kind_dir}/${AIOT_BUILD_MARKER}"
expect_failure "wrong build kind marker" \
    aiot_prepare_build_directory "${project_root}" "${wrong_kind_dir}" "product" true
expect_preserved "${wrong_kind_dir}/keep"

managed_dir="${test_root}/managed"
aiot_prepare_build_directory "${project_root}" "${managed_dir}" "product" false
touch "${managed_dir}/remove-me"
aiot_prepare_build_directory "${project_root}" "${managed_dir}" "product" true

if [[ -e "${managed_dir}/remove-me" ]]; then
    echo "Managed directory was not cleaned" >&2
    exit 1
fi

if [[ ! -f "${managed_dir}/${AIOT_BUILD_MARKER}" ]]; then
    echo "Managed directory marker was not recreated" >&2
    exit 1
fi

echo "Build directory guard tests passed"

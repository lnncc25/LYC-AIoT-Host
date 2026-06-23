#!/usr/bin/env bash

readonly AIOT_BUILD_MARKER=".aiot-build-directory"

aiot_is_protected_path()
{
    local candidate="$1"
    local protected_project_root="$2"

    [[ "${candidate}" == "${protected_project_root}" \
        || "${candidate}" == "${protected_project_root}/"* \
        || "${protected_project_root}" == "${candidate}/"* ]]
}

aiot_marker_content()
{
    local marker_project_root="$1"
    local build_kind="$2"

    printf 'project_root=%s\nbuild_kind=%s\n' "${marker_project_root}" "${build_kind}"
}

aiot_prepare_build_directory()
{
    local canonical_project_root
    local canonical_build_dir
    local build_kind="$3"
    local clean_build="$4"
    local marker_path
    local expected_marker
    local actual_marker

    canonical_project_root="$(realpath -m "$1")"
    canonical_build_dir="$(realpath -m "$2")"
    marker_path="${canonical_build_dir}/${AIOT_BUILD_MARKER}"
    expected_marker="$(aiot_marker_content "${canonical_project_root}" "${build_kind}")"

    if aiot_is_protected_path "${canonical_build_dir}" "${canonical_project_root}"; then
        echo "Refusing protected build directory: ${canonical_build_dir}" >&2
        return 2
    fi

    if [[ -e "${canonical_build_dir}" && ! -d "${canonical_build_dir}" ]]; then
        echo "Build path is not a directory: ${canonical_build_dir}" >&2
        return 2
    fi

    if [[ "${clean_build}" == true && -e "${canonical_build_dir}" ]]; then
        if [[ ! -f "${marker_path}" ]]; then
            echo "Refusing to clean unmarked directory: ${canonical_build_dir}" >&2
            return 2
        fi

        actual_marker="$(cat "${marker_path}")"
        if [[ "${actual_marker}" != "${expected_marker}" ]]; then
            echo "Refusing to clean directory with mismatched marker: ${canonical_build_dir}" >&2
            return 2
        fi

        rm -rf -- "${canonical_build_dir}"
    elif [[ -e "${canonical_build_dir}" ]]; then
        if [[ ! -f "${marker_path}" ]]; then
            echo "Refusing existing directory not created by this script: ${canonical_build_dir}" >&2
            return 2
        fi
        echo "Build directory already exists: ${canonical_build_dir}" >&2
        echo "Use --clean to rebuild it explicitly." >&2
        return 2
    fi

    mkdir -p "${canonical_build_dir}"
    aiot_marker_content "${canonical_project_root}" "${build_kind}" > "${marker_path}"
}

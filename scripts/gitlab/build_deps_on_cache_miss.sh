#!/usr/bin/env bash

set -o errexit
set -o nounset

exec 2>&1

project_dir=${PROJECT_DIR:-""}
spec=${SPEC:-""}
prefix=${PREFIX:-""}
spack_debug=${SPACK_DEBUG:-false}
push_to_registry=${PUSH_TO_REGISTRY:-false}
cache_target=${CACHE_TARGET:-""}
cache_key=${CACHE_KEY:-""}
umpire_ci_storage_root=${UMPIRE_CI_STORAGE_ROOT:-/usr/workspace/umpire/ci-cache}
umpire_ci_storage_group=${UMPIRE_CI_STORAGE_GROUP:-umpire}
umpire_ci_storage_umask=${UMPIRE_CI_STORAGE_UMASK:-0002}
umpire_ci_upstream_target=${UMPIRE_CI_UPSTREAM_TARGET:-develop}
ci_registry_image=${CI_REGISTRY_IMAGE:-"czregistry.llnl.gov:5050/radiuss/umpire"}
truehostname="$(hostname)"
truehostname="${truehostname//[0-9]/}"
export ci_registry_user=${CI_REGISTRY_USER:-"${USER}"}
export ci_registry_token=${CI_REGISTRY_TOKEN:-""}
. "${project_dir}/scripts/gitlab/gitlab_logs_helpers.bash"

ensure_storage_dir ()
{
    local dir_path="${1}"
    mkdir -p "${dir_path}"
    if [[ -n "${umpire_ci_storage_group}" ]]
    then
        chgrp "${umpire_ci_storage_group}" "${dir_path}" 2>/dev/null || \
          print_warning "Unable to set group ${umpire_ci_storage_group} on ${dir_path}"
    fi
    chmod g+rwxs "${dir_path}" 2>/dev/null || \
      print_warning "Unable to set group writable permissions on ${dir_path}"
}

set_storage_file_permissions ()
{
    local file_path="${1}"
    if [[ -n "${umpire_ci_storage_group}" ]]
    then
        chgrp "${umpire_ci_storage_group}" "${file_path}" 2>/dev/null || \
          print_warning "Unable to set group ${umpire_ci_storage_group} on ${file_path}"
    fi
    chmod g+rw "${file_path}" 2>/dev/null || \
      print_warning "Unable to set group writable permissions on ${file_path}"
}

cache_root_for ()
{
    local target="${1}"
    printf '%s/%s/%s/%s' \
      "${umpire_ci_storage_root}" \
      "${SYS_TYPE:-unknown}" \
      "${CI_MACHINE:-${truehostname}}" \
      "${target}"
}

run_spack ()
{
    local cmd=("${prefix}/spack/bin/spack")
    if [[ ${spack_debug} == true ]]
    then
        cmd+=("--debug" "--stacktrace")
    fi
    "${cmd[@]}" "$@"
}

run_uberenv ()
{
    local cmd=("${project_dir}/scripts/uberenv/uberenv.py")
    if [[ ${spack_debug} == true ]]
    then
        cmd+=("--spack-debug")
    fi
    "${cmd[@]}" "$@"
}

clean_hostconfig ()
{
    # Clear stale host-config from any prior build before generating a fresh one.
    shopt -s nullglob
    stale_cmake=( "${prefix}"/*.cmake )
    shopt -u nullglob
    if [[ ${#stale_cmake[@]} -gt 0 ]]
    then
        print_info "Removing stale host-config files: ${stale_cmake[*]}"
        rm -f "${stale_cmake[@]}"
    fi
}

find_project_hostconfig ()
{
    local hostconfigs=()
    shopt -s nullglob
    hostconfigs=( "${prefix}"/*.cmake )
    shopt -u nullglob

    if [[ ${#hostconfigs[@]} -ne 1 ]]
    then
        print_error "Expected exactly one .cmake file in ${prefix}, found ${#hostconfigs[@]}:"
        printf '%s\n' "${hostconfigs[@]}"
        return 1
    fi

    printf '%s\n' "${hostconfigs[0]}"
}

configure_spack_storage ()
{
    local common_config upstream_config
    local cache_root cache_install_tree cache_buildcache upstream_install_tree spack_db_dir
    common_config="${project_dir}/scripts/gitlab/umpire-ci-cache-common.yaml"
    upstream_config="${project_dir}/scripts/gitlab/umpire-ci-cache-upstream.yaml"
    cache_root="$(cache_root_for "${cache_target}")"
    cache_install_tree="${cache_root}/install"
    cache_buildcache="${cache_root}/buildcache"
    upstream_install_tree="$(cache_root_for "${umpire_ci_upstream_target}")/install"
    # Spack may pad install_tree with __spack_path_placeholder__ segments.
    spack_db_dir="$(find "${upstream_install_tree}" -mindepth 1 -maxdepth 8 -type d -name .spack-db 2>/dev/null | head -n 1 || true)"
    if [[ -n "${spack_db_dir}" ]]
    then
        if [[ "$(dirname "${spack_db_dir}")" != "${upstream_install_tree}" ]]
        then
            print_info "Resolved padded upstream install tree: $(dirname "${spack_db_dir}")"
        fi
        upstream_install_tree="$(dirname "${spack_db_dir}")"
    fi

    export UMPIRE_CI_INSTALL_TREE="${cache_install_tree}"
    export UMPIRE_CI_BUILDCACHE="${cache_buildcache}"
    export UMPIRE_CI_BUILDCACHE_URL="file://${cache_buildcache}"
    export UMPIRE_CI_UPSTREAM_INSTALL_TREE="${upstream_install_tree}"
    export UMPIRE_CI_UPSTREAM_TARGET="${umpire_ci_upstream_target}"
    export UMPIRE_CI_STORAGE_GROUP="${umpire_ci_storage_group}"

    ensure_storage_dir "${cache_install_tree}"
    ensure_storage_dir "${cache_buildcache}"
    ensure_storage_dir "${cache_root}/host-configs"

    run_section "spack_filesystem_cache" "Filesystem Spack cache configuration" "collapsed" \
      "Configuring filesystem Spack cache failed" \
      run_spack -D "${prefix}/spack_env" config add "include:${common_config}"

    # Inject the configurable storage group into the Spack packages config.
    # Shell variables are not expanded by Spack when reading YAML config files,
    # so we use 'spack config add' to set the value programmatically.
    if [[ -n "${umpire_ci_storage_group}" ]]
    then
        run_spack -D "${prefix}/spack_env" config add \
          "packages:all:permissions:group:${umpire_ci_storage_group}"
    fi

    # Non-upstream branches reuse the upstream install tree to minimize rebuilds.
    if [[ "${cache_target}" != "${umpire_ci_upstream_target}" ]] && \
       [[ -d "${upstream_install_tree}" && -d "${upstream_install_tree}/.spack-db" ]]
    then
        run_spack -D "${prefix}/spack_env" config add "include:${upstream_config}"
        print_info "Using ${umpire_ci_upstream_target} install tree as Spack upstream: ${upstream_install_tree}"
    fi
}

publish_cached_hostconfig ()
{
    local generated_hostconfig="${1}"
    local cache_root target_hostconfig tmp_hostconfig local_hostconfig
    cache_root="$(cache_root_for "${cache_target}")"
    target_hostconfig="${cache_root}/host-configs/${cache_key}.cmake"
    tmp_hostconfig="${target_hostconfig}.tmp.$$"
    local_hostconfig="${project_dir}/${cache_key}.cmake"

    cp "${generated_hostconfig}" "${tmp_hostconfig}"
    mv "${tmp_hostconfig}" "${target_hostconfig}"
    set_storage_file_permissions "${target_hostconfig}"
    # Keep a deterministic local name so parent script can use it directly.
    cp "${target_hostconfig}" "${local_hostconfig}"

    print_info "Published cached host-config: ${target_hostconfig}"
    print_info "Materialized host-config path: ${local_hostconfig}"
}

###############################################################################
# SCRIPT CORE
###############################################################################

if [[ -z "${spec}" ]]
then
    print_error "SPEC is undefined, aborting..."
    return 1
fi

if [[ ! -d "${prefix}" ]]
then
    print_error "PREFIX ${prefix} does not exist or is not a directory, aborting..."
    return 1
fi

# Ensure shared filesystem artifacts keep group-writable permissions.
umask "${umpire_ci_storage_umask}"
spack_user_cache="${prefix}/spack-user-cache"
export SPACK_DISABLE_LOCAL_CONFIG=""
export SPACK_USER_CACHE_PATH="${spack_user_cache}"
mkdir -p "${spack_user_cache}"

run_section "spack_setup" "Spack setup and environment" "collapsed" \
  "Spack environment setup failed (Uberenv)" \
  run_uberenv --setup-and-env-only --spec="${spec}" --prefix="${prefix}"

configure_spack_storage

if [[ -n "${ci_registry_token}" && ${push_to_registry} == true ]]
then
    run_section "registry_setup" "GitLab registry as Spack Buildcache" "collapsed" \
      "Adding gitlab registry to spack environment failed" \
      run_spack -D "${prefix}/spack_env" mirror add --unsigned --oci-username-variable ci_registry_user --oci-password-variable ci_registry_token gitlab_ci "oci://${ci_registry_image}"
fi

clean_hostconfig

run_section "spack_build" "Spack build of dependencies" "collapsed" \
  "Spack build of dependencies failed (Uberenv)" \
  run_uberenv --skip-setup-and-env --spec="${spec}" --prefix="${prefix}"

generated_hostconfig="$(find_project_hostconfig)" || return 1

# Push dependencies and publish a host-config keyed by cache identity.
run_section "filesystem_buildcache_push" "Push dependencies to filesystem buildcache" "collapsed" \
  "Pushing dependencies to filesystem buildcache failed" \
  run_spack -D "${prefix}/spack_env" buildcache push --only dependencies --unsigned --update-index umpire_ci_buildcache

publish_cached_hostconfig "${generated_hostconfig}"

if [[ -n "${ci_registry_token}" && ${push_to_registry} == true ]]
then
    run_section "registry_buildcache_push" "Push dependencies to GitLab registry buildcache" "collapsed" \
      "Pushing dependencies to gitlab registry failed" \
      run_spack -D "${prefix}/spack_env" buildcache push --only dependencies gitlab_ci
fi

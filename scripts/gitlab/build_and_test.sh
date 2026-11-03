#!/usr/bin/env bash

# Initialize modules for users not using bash as a default shell
if test -e /usr/share/lmod/lmod/init/bash
then
  . /usr/share/lmod/lmod/init/bash
fi

###############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
###############################################################################

# Navigation:
# - VARIABLES
# - HELPER FUNCTIONS
# - SETUP
# - BUILD DEPENDENCIES
# - HOST CONFIG / CMAKE CACHE FILES
# - BUILD PROJECT
# - TEST PROJECT

###############################################################################
# VARIABLES
###############################################################################

set -o errexit
set -o nounset

exec 2>&1

option=${1:-""}
hostname="$(hostname)"
truehostname=${hostname//[0-9]/}
project_dir="$(pwd)"
. "${project_dir}/scripts/gitlab/gitlab_logs_helpers.bash"

hostconfig=${HOST_CONFIG:-""}
hostconfig_path=""
spec=${SPEC:-""}
module_list=${MODULE_LIST:-""}
job_unique_id=${CI_JOB_ID:-""}
use_dev_shm=${USE_DEV_SHM:-true}
spack_debug=${SPACK_DEBUG:-false}
debug_mode=${DEBUG_MODE:-false}
push_to_registry=${PUSH_TO_REGISTRY:-false}
# PUSH_TO_REGISTRY defaults to false: the persistent filesystem install tree
# (configured via UMPIRE_CI_STORAGE_ROOT) is used as a Spack upstream, which
# is faster than a binary buildcache and requires no GitLab token for read
# access. Set PUSH_TO_REGISTRY=true to additionally push to the OCI registry.
umpire_ci_storage_root=${UMPIRE_CI_STORAGE_ROOT:-/usr/workspace/umpire/ci-cache}
umpire_ci_storage_group=${UMPIRE_CI_STORAGE_GROUP:-umpire}
umpire_ci_storage_umask=${UMPIRE_CI_STORAGE_UMASK:-0002}
umpire_ci_force_spack=${UMPIRE_CI_FORCE_SPACK:-false}
umpire_ci_cache_target=${UMPIRE_CI_CACHE_TARGET:-""}
umpire_ci_upstream_target=${UMPIRE_CI_UPSTREAM_TARGET:-develop}

# REGISTRY_TOKEN allows you to provide your own personal access token to the CI
# registry. Be sure to set the token with at least read access to the registry.
registry_token=${REGISTRY_TOKEN:-""}
ci_registry_image=${CI_REGISTRY_IMAGE:-"czregistry.llnl.gov:5050/radiuss/umpire"}
export ci_registry_user=${CI_REGISTRY_USER:-"${USER}"}
export ci_registry_token=${CI_JOB_TOKEN:-"${registry_token}"}

cache_key=""
cache_target=""

###############################################################################
# HELPER FUNCTIONS
###############################################################################

sha256_hex ()
{
    if command -v sha256sum >/dev/null 2>&1
    then
        sha256sum | awk '{print $1}'
    else
        shasum -a 256 | awk '{print $1}'
    fi
}

git_commit ()
{
    local repo_path="${1}"
    git -C "${repo_path}" rev-parse HEAD 2>/dev/null || echo unknown
}

resolve_cache_target ()
{
    local upstream_target="${umpire_ci_upstream_target}"

    if [[ -n "${umpire_ci_cache_target}" ]]
    then
        echo "${umpire_ci_cache_target}"
    elif [[ "${CI_COMMIT_BRANCH:-}" == "${CI_DEFAULT_BRANCH:-${upstream_target}}" ]]
    then
        echo "${upstream_target}"
    elif [[ -n "${CI_COMMIT_REF_SLUG:-}" ]]
    then
        echo "ref-${CI_COMMIT_REF_SLUG}"
    else
        echo "manual"
    fi
}

resolve_cache_key ()
{
    # Cache identity captures inputs that materially affect dependency resolution.
    printf '%s\n' \
      "cache-format=umpire-ci-v1" \
      "spec=${spec}" \
      "module-list=${module_list}" \
      "sys-type=${SYS_TYPE:-unknown}" \
      "machine=${CI_MACHINE:-${truehostname}}" \
      "uberenv-config-hash=$(sha256_hex < "${project_dir}/.uberenv_config.json")" \
      "uberenv-commit=$(git_commit "${project_dir}/scripts/uberenv")" \
      "radiuss-spack-configs-commit=$(git_commit "${project_dir}/scripts/radiuss-spack-configs")" | \
      sha256_hex
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

resolve_cache_context ()
{
    cache_target="$(resolve_cache_target)"
    cache_key="$(resolve_cache_key)"
    local cache_root cache_install_tree cache_buildcache cache_hostconfigs_dir
    cache_root="$(cache_root_for "${cache_target}")"
    cache_install_tree="${cache_root}/install"
    cache_buildcache="${cache_root}/buildcache"
    cache_hostconfigs_dir="${cache_root}/host-configs"

    umask "${umpire_ci_storage_umask}"
    ensure_storage_dir "${cache_install_tree}"
    ensure_storage_dir "${cache_buildcache}"
    ensure_storage_dir "${cache_hostconfigs_dir}"

    print_info "Umpire CI cache target: ${cache_target}"
    print_info "Umpire CI cache key: ${cache_key}"
    print_info "Umpire CI cache root: ${cache_root}"
}

resolve_cached_hostconfig ()
{
    # Cache-read is read-only: try the branch target first, then upstream.
    local targets=("${cache_target}")
    if [[ "${cache_target}" != "${umpire_ci_upstream_target}" ]]
    then
        targets+=("${umpire_ci_upstream_target}")
    fi

    local target cache_root cache_install_tree cache_hostconfig_path spack_db_dir
    for target in "${targets[@]}"
    do
        cache_root="$(cache_root_for "${target}")"
        cache_install_tree="${cache_root}/install"
        # Spack may pad install_tree with __spack_path_placeholder__ segments.
        spack_db_dir="$(find "${cache_install_tree}" -mindepth 1 -maxdepth 8 -type d -name .spack-db 2>/dev/null | head -n 1 || true)"
        if [[ -n "${spack_db_dir}" ]]
        then
            if [[ "$(dirname "${spack_db_dir}")" != "${cache_install_tree}" ]]
            then
                print_info "Resolved padded install tree for ${target}: $(dirname "${spack_db_dir}")"
            fi
            cache_install_tree="$(dirname "${spack_db_dir}")"
        fi
        cache_hostconfig_path="${cache_root}/host-configs/${cache_key}.cmake"

        if [[ "${umpire_ci_force_spack}" != true ]] && \
           [[ -f "${cache_hostconfig_path}" ]] && \
           [[ -d "${cache_install_tree}" && -d "${cache_install_tree}/.spack-db" ]]
        then
            # Materialize a deterministic local path for downstream CMake steps.
            cp "${cache_hostconfig_path}" "${project_dir}/$(basename "${cache_hostconfig_path}")"
            hostconfig="$(basename "${cache_hostconfig_path}")"
            hostconfig_path="${project_dir}/${hostconfig}"
            print_info "Using cached host-config from ${target}: ${cache_hostconfig_path}"
            return 0
        fi
    done

    return 1
}

###############################################################################
# SETUP
###############################################################################

if [[ ${debug_mode} == true ]]
then
    print_info "Debug mode:"
    print_info "- Spack debug mode."
    print_info "- Deactivated shared memory."
    print_info "- Do not push to buildcache."
    use_dev_shm=false
    spack_debug=true
    push_to_registry=false
fi

if [[ -n ${module_list} ]]
then
    print_info "Loading modules: ${module_list}"
    module load ${module_list}
fi

prefix=""

if [[ -d /dev/shm && ${use_dev_shm} == true ]]
then
    prefix="/dev/shm/${hostname}"
    if [[ -z ${job_unique_id} ]]; then
      job_unique_id=manual_job_$(date +%s)
      while [[ -d ${prefix}-${job_unique_id} ]] ; do
          sleep 1
          job_unique_id=manual_job_$(date +%s)
      done
    fi

    prefix="${prefix}-${job_unique_id}"
else
    # We set the prefix in the parent directory so that spack dependencies are not installed inside the source tree.
    prefix="${project_dir}/../spack-and-build-root"
fi

print_info "Creating directory ${prefix}"
print_info "project_dir: ${project_dir}"

mkdir -p ${prefix}

###############################################################################
# BUILD DEPENDENCIES
###############################################################################
if [[ "${option}" != "--build-only" && "${option}" != "--test-only" ]]
then
    section_start "dependencies" "Building Dependencies"

    if [[ -z ${spec} ]]
    then
        section_end ; print_error "SPEC is undefined, aborting..."
        exit 1
    fi

    # Get a hostconfig file.
    if [[ -n "${hostconfig}" ]]
    then
        # Scenario 1: HOST_CONFIG explicitly provided by caller.
        if [[ -f "${hostconfig}" ]]
        then
            hostconfig_path="${hostconfig}"
        elif [[ -f "${project_dir}/${hostconfig}" ]]
        then
            hostconfig_path="${project_dir}/${hostconfig}"
        else
            section_end ; print_error "HOST_CONFIG is set but file does not exist: ${hostconfig}"
            exit 1
        fi
        print_info "HOST_CONFIG is set; skipping dependency installation and using provided host-config"
    else
        # Scenario 2: no HOST_CONFIG, so resolve cache identity and try read-only reuse.
        resolve_cache_context
        if resolve_cached_hostconfig
        then
            print_info "Cache hit; skipping dependency installation"
        else
            # Scenario 3: cache miss, so build dependencies and publish cache artifacts.
            export PROJECT_DIR="${project_dir}"
            export PREFIX="${prefix}"
            export SPEC="${spec}"
            export SPACK_DEBUG="${spack_debug}"
            export CACHE_TARGET="${cache_target}"
            export CACHE_KEY="${cache_key}"
            export UMPIRE_CI_STORAGE_ROOT="${umpire_ci_storage_root}"
            export UMPIRE_CI_STORAGE_GROUP="${umpire_ci_storage_group}"
            export UMPIRE_CI_STORAGE_UMASK="${umpire_ci_storage_umask}"
            export UMPIRE_CI_UPSTREAM_TARGET="${umpire_ci_upstream_target}"
            export PUSH_TO_REGISTRY="${push_to_registry}"
            export CI_REGISTRY_IMAGE="${ci_registry_image}"
            export CI_REGISTRY_USER="${ci_registry_user}"
            export CI_REGISTRY_TOKEN="${ci_registry_token}"

            run_section "cache_miss" "Building dependencies on cache miss" "collapsed" \
              "Spack dependency build failed" \
              bash "${project_dir}/scripts/gitlab/build_deps_on_cache_miss.sh"

            # Cache-miss script publishes host-config as <cache_key>.cmake.
            hostconfig="${cache_key}.cmake"
            hostconfig_path="${project_dir}/${hostconfig}"
            if [[ ! -f "${hostconfig_path}" ]]
            then
                section_end ; print_error "Expected generated host-config not found: ${hostconfig_path}"
                exit 1
            fi
        fi
    fi

    section_end
fi

###############################################################################
# HOST CONFIG / CMAKE CACHE FILE
###############################################################################
if [[ -z "${hostconfig_path}" ]]
then
    print_error "Host-config path is undefined. Provide HOST_CONFIG or run dependency setup."
    exit 1
fi

hostconfig=$(basename ${hostconfig_path})
print_info "Found hostconfig ${hostconfig_path}"

###############################################################################
# BUILD PROJECT
###############################################################################
# When using /dev/shm, we use prefix for both spack builds and source build, unless BUILD_ROOT was defined
build_root=${BUILD_ROOT:-"${prefix}"}

build_dir="${build_root}/build_${hostconfig//.cmake/}"
install_dir="${build_root}/install_${hostconfig//.cmake/}"

cmake_exe=$(grep 'CMake executable' ${hostconfig_path} | cut -d ':' -f 2 | xargs 2>/dev/null)

if [[ "${option}" != "--deps-only" && "${option}" != "--test-only" ]]
then
    print_info "Prefix       ${prefix}"
    print_info "Host-config  ${hostconfig_path}"
    print_info "Build Dir    ${build_dir}"
    print_info "Project Dir  ${project_dir}"
    print_info "Install Dir  ${install_dir}"

    section_start "clean" "Cleaning working directory" "collapsed"
    # Map CPU core allocations
    declare -A core_counts=(["lassen"]=40 ["poodle"]=28 ["dane"]=28 ["matrix"]=28 ["corona"]=32 ["rzansel"]=48 ["tioga"]=32 ["tuolumne"]=48)

    # If building, then delete everything first
    # NOTE: 'cmake --build . -j core_counts' attempts to reduce individual build resources.
    #       If core_counts does not contain hostname, then will default to '-j ', which should
    #       use max cores.
    rm -rf ${build_dir} 2>/dev/null
    mkdir -p ${build_dir} && cd ${build_dir}
    section_end

    # We set the MPI tests command to allow overlapping.
    # Shared allocation: Allows build_and_test.sh to run within a sub-allocation (see CI config).
    # Use /dev/shm: Prevent MPI tests from running on a node where the build dir doesn't exist.
    cmake_options=""
    if [[ "${truehostname}" == "dane" || "${truehostname}" == "poodle" ]]
    then
        cmake_options="-DBLT_MPI_COMMAND_APPEND:STRING=--overlap"
    fi

    section_start "cmake_config" "CMake Configuration" "collapsed"
    if $cmake_exe \
      -C ${hostconfig_path} \
      ${cmake_options} \
      -DCMAKE_INSTALL_PREFIX=${install_dir} \
      ${project_dir}
    then
        section_end
    else
        status=$?
        section_end ; print_error "CMake configuration failed, dumping output..."

        $cmake_exe \
          -C ${hostconfig_path} \
          ${cmake_options} \
          -DCMAKE_INSTALL_PREFIX=${install_dir} \
          ${project_dir} --debug-output --trace-expand

        exit ${status}
    fi

    section_start "build" "Building Umpire" "collapsed"
    if $cmake_exe --build . -j ${core_counts[$truehostname]}
    then
        section_end
    else
        status=$?
        section_end ; print_error "Compilation failed, building with verbose output..."

        section_start "build_verbose" "Verbose Rebuild"
        $cmake_exe --build . --verbose -j 1
        section_end

        exit ${status}
    fi

    run_section "install" "Installing Umpire" "collapsed" \
      "Installation failed" \
      $cmake_exe --install .
fi

###############################################################################
# TEST PROJECT
###############################################################################
if [[ "${option}" != "--build-only" ]] && grep -q -i "ENABLE_TESTS.*ON" ${hostconfig_path}
then

    if [[ ! -d ${build_dir} ]]
    then
        print_error "Build directory not found : ${build_dir}"
        exit 1
    fi

    cd ${build_dir}

    section_start "tests" "Running Tests" "collapsed"
    ctest --output-on-failure --no-compress-output -T test -VV 2>&1 | tee tests_output.txt
    ctest_status=${PIPESTATUS[0]}

    # If Developer benchmarks enabled, run the no-op benchmark and show output
    if [[ "${option}" != "--build-only" ]] && grep -q -i "UMPIRE_ENABLE_DEVELOPER_BENCHMARKS.*ON" ${hostconfig_path}
    then
        ctest --verbose -C Benchmark -R no-op_stress_test
    fi

    no_test_str="No tests were found!!!"
    if [[ "$(tail -n 1 tests_output.txt)" == "${no_test_str}" ]]
    then
        section_end ; print_error "No tests were found (ctest status: ${ctest_status})"
        exit 1
    fi

    tree Testing
    xsltproc -o junit.xml ${project_dir}/scripts/radiuss-spack-configs/utilities/ctest-to-junit.xsl Testing/*/Test.xml
    mv junit.xml ${project_dir}/junit.xml

    if grep -q "Errors while running CTest" ./tests_output.txt
    then
        section_end ; print_error "Failure(s) while running CTest (ctest status: ${ctest_status})"
        exit 1
    fi
    section_end

    section_start "install_test" "Testing Installed Examples" "collapsed"
    if grep -q -i "ENABLE_HIP.*ON" ${hostconfig_path}
    then
        section_end ; print_warning "Not testing install with HIP"
    else
        if [[ ! -d ${install_dir} ]]
        then
            section_end ; print_error "Install directory not found : ${install_dir}"
            exit 1
        fi

        cd ${install_dir}/examples/umpire/using-with-cmake
        mkdir build && cd build
        if ! $cmake_exe -C ../host-config.cmake ..; then
            section_end ; print_error "Running $cmake_exe for using-with-cmake test"
            exit 1
        fi

        if ! make; then
            section_end ; print_error "Running make for using-with-cmake test"
            exit 1
        fi
        section_end
    fi
fi

#timed_message "Cleaning up"
#make clean

cd ${project_dir}

echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "~ Build and test completed"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

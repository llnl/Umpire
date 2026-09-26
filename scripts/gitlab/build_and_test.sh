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

hostconfig=${HOST_CONFIG:-""}
spec=${SPEC:-""}
module_list=${MODULE_LIST:-""}
job_unique_id=${CI_JOB_ID:-""}
use_dev_shm=${USE_DEV_SHM:-true}
spack_debug=${SPACK_DEBUG:-false}
debug_mode=${DEBUG_MODE:-false}
push_to_registry=${PUSH_TO_REGISTRY:-true}

# REGISTRY_TOKEN allows you to provide your own personal access token to the CI
# registry. Be sure to set the token with at least read access to the registry.
registry_token=${REGISTRY_TOKEN:-""}
ci_registry_image=${CI_REGISTRY_IMAGE:-"czregistry.llnl.gov:5050/radiuss/umpire"}
export ci_registry_user=${CI_REGISTRY_USER:-"${USER}"}
export ci_registry_token=${CI_JOB_TOKEN:-"${registry_token}"}

###############################################################################
# HELPER FUNCTIONS
###############################################################################

# Helper function to print errors in red
print_error ()
{
    local error_msg="${1}"
    echo -e "\e[31m[Error]: ${error_msg}\e[0m"
}

# Helper function to print warnings in gray
print_warning ()
{
    local warning_msg="${1}"
    echo -e "\e[1;30m[Warning]: ${warning_msg}\e[0m"
}

# Helper function to print information
print_info ()
{
    local info_msg="${1}"
    echo -e "[Information]: ${info_msg}"
}

# Portable UTC timestamp formatter for epoch seconds.
format_utc_timestamp ()
{
    local timestamp="${1}"
    # BSD/macOS date supports epoch conversion via: date -r <seconds>
    if date -u -r "${timestamp}" "+%Y-%m-%d %H:%M:%S UTC" >/dev/null 2>&1
    then
        date -u -r "${timestamp}" "+%Y-%m-%d %H:%M:%S UTC"
    else
        # GNU date supports epoch conversion via: date -d "@<seconds>"
        date -u -d "@${timestamp}" "+%Y-%m-%d %H:%M:%S UTC"
    fi
}

# Portable elapsed time formatter (HH:MM:SS).
format_elapsed_hms ()
{
    local elapsed="${1}"
    printf '%02d:%02d:%02d' $((elapsed / 3600)) $(((elapsed % 3600) / 60)) $((elapsed % 60))
}

# Write the accumulated section timings as a machine-readable JSON file. This is
# registered as an EXIT trap so that timings are captured even when the build
# fails: each completed section is recorded by section_end before any early
# exit. Built with printf to avoid a jq dependency. Must not change the script
# exit status, so the incoming code is captured first and never overridden.
write_timings_file ()
{
    local exit_code=$?
    local timings_file="${project_dir}/timings.json"
    local now=$(date +%s)
    local total_seconds=$((now - script_start_time))

    {
        printf '{\n'
        printf '  "schema": 1,\n'
        printf '  "sha": "%s",\n' "${CI_COMMIT_SHA:-}"
        printf '  "short_sha": "%s",\n' "${CI_COMMIT_SHORT_SHA:-}"
        printf '  "ts": %s,\n' "${script_start_time}"
        printf '  "machine": "%s",\n' "${CI_MACHINE:-${truehostname}}"
        printf '  "job": "%s",\n' "${CI_JOB_NAME:-}"
        printf '  "spec": "%s",\n' "${spec//\"/\\\"}"
        printf '  "pipeline_id": "%s",\n' "${CI_PIPELINE_ID:-}"
        printf '  "exit_code": %s,\n' "${exit_code}"
        printf '  "total_seconds": %s,\n' "${total_seconds}"
        printf '  "sections": ['

        local first=true
        local record
        for record in "${timing_records[@]:-}"
        do
            [[ -z "${record}" ]] && continue
            local name="${record%%|*}"
            local rest="${record#*|}"
            local depth="${rest%%|*}"
            rest="${rest#*|}"
            local parent="${rest%%|*}"
            local seconds="${rest##*|}"
            if [[ "${first}" == true ]]
            then
                first=false
                printf '\n'
            else
                printf ',\n'
            fi
            printf '    {"name": "%s", "depth": %s, "parent": "%s", "seconds": %s}' \
                "${name}" "${depth}" "${parent}" "${seconds}"
        done
        if [[ "${first}" == false ]]
        then
            printf '\n  ]\n'
        else
            printf ']\n'
        fi
        printf '}\n'
    } > "${timings_file}" 2>/dev/null || print_warning "Failed to write timings file"

    print_info "Wrote section timings to ${timings_file}"
}

# Track script start time for elapsed time calculations
script_start_time=$(date +%s)

# Always emit machine-readable section timings on exit (success or failure).
trap write_timings_file EXIT

# Storage for section start times (supports nesting)
declare -A section_start_times

# Storage for section metadata, used to emit machine-readable timings.
declare -A section_names
declare -A section_depths
declare -A section_parents

# Section stack for tracking nested sections
section_id_stack=()
section_counter=0
section_indent=""

# Accumulated per-section timing records ("name|depth|parent|seconds"), one per
# completed section. Consumed by write_timings_file at script exit.
timing_records=()

# GitLab CI collapsible section helpers with nesting support
section_start ()
{
    local section_name="${1}"
    local section_title="${2}"
    local section_state="${3:-""}"

    local collapsed="false"
    if [[ "${section_state}" == "collapsed" ]]
    then
        collapsed="true"
    fi

    # Generate unique section ID
    section_counter=$((section_counter + 1))
    local section_id="${section_name}_${section_counter}"

    local timestamp=$(date +%s)
    local current_time=$(format_utc_timestamp "${timestamp}")
    local total_elapsed=$((timestamp - script_start_time))
    local total_elapsed_formatted=$(format_elapsed_hms "${total_elapsed}")

    # Store section start time for later calculation
    section_start_times[${section_id}]=${timestamp}

    # Store section metadata for machine-readable timings. The depth is the
    # current stack size and the parent is the section currently on top of the
    # stack (empty for top-level sections), both captured before pushing.
    section_names[${section_id}]="${section_title}"
    section_depths[${section_id}]=${#section_id_stack[@]}
    if [[ ${#section_id_stack[@]} -gt 0 ]]
    then
        local parent_id="${section_id_stack[$((${#section_id_stack[@]} - 1))]}"
        section_parents[${section_id}]="${section_names[${parent_id}]:-}"
    else
        section_parents[${section_id}]=""
    fi

    # Push section ID onto stack
    section_id_stack+=("${section_id}")

    echo -e "\e[1;30m${section_indent}~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\e[0m"
    echo -e "\e[1;30m${section_indent}~ TIME                    | TOTAL    | SECTION  \e[0m"
    echo -e "\e[1;30m${section_indent}~ ${current_time} | ${total_elapsed_formatted} | ${section_title}\e[0m"
    echo -e "\e[0Ksection_start:${timestamp}:${section_id}[collapsed=${collapsed}]\r\e[0K${section_indent}~ ${section_title}"

    # Increase indentation for nested sections
    section_indent="${section_indent}  "
}

section_end ()
{
    # Pop section ID from stack
    if [[ ${#section_id_stack[@]} -eq 0 ]]; then
        print_warning "section_end called with empty stack"
        return 1
    fi

    # Decrease indentation before displaying
    section_indent="${section_indent%  }"

    local stack_index=$((${#section_id_stack[@]} - 1))
    local section_id="${section_id_stack[$stack_index]}"
    unset section_id_stack[$stack_index]

    local timestamp=$(date +%s)
    local current_time=$(format_utc_timestamp "${timestamp}")
    local total_elapsed=$((timestamp - script_start_time))
    local total_elapsed_formatted=$(format_elapsed_hms "${total_elapsed}")

    # Calculate section elapsed time
    local section_start=${section_start_times[${section_id}]:-${timestamp}}
    local section_elapsed=$((timestamp - section_start))
    local section_elapsed_formatted=$(format_elapsed_hms "${section_elapsed}")

    echo -e "\e[0Ksection_end:${timestamp}:${section_id}\r\e[0K\e[0m"
    echo -e "\e[1;30m${section_indent}~ ${current_time} | ${total_elapsed_formatted} | ${section_elapsed_formatted}\e[0m"
    echo -e "\e[1;30m${section_indent}~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\e[0m"

    # Record a machine-readable timing for this section.
    timing_records+=("${section_names[${section_id}]:-${section_id}}|${section_depths[${section_id}]:-0}|${section_parents[${section_id}]:-}|${section_elapsed}")

    # Clean up stored data
    unset section_start_times[${section_id}]
    unset section_names[${section_id}]
    unset section_depths[${section_id}]
    unset section_parents[${section_id}]
}

# For convenience, a helper function to run a command within a section and handle errors
run_section() {
    local id="$1"
    local title="$2"
    local collapsed="$3"
    local err_msg="$4"
    local status=0
    shift 4

    section_start "$id" "$title" "$collapsed"
    if "$@"; then
        section_end
    else
        status=$?
        section_end
        print_error "$err_msg"
        exit $status
    fi
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

spack_cmd="${prefix}/spack/bin/spack"
spack_env_path="${prefix}/spack_env"
uberenv_cmd="${project_dir}/scripts/uberenv/uberenv.py"
if [[ ${spack_debug} == true ]]
then
    spack_cmd="${spack_cmd} --debug --stacktrace"
    uberenv_cmd="${uberenv_cmd} --spack-debug"
fi

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

    prefix_opt="--prefix=${prefix}"

    # We force Spack to put all generated files (cache and configuration of
    # all sorts) in a unique location so that there can be no collision
    # with existing or concurrent Spack.
    spack_user_cache="${prefix}/spack-user-cache"
    export SPACK_DISABLE_LOCAL_CONFIG=""
    export SPACK_USER_CACHE_PATH="${spack_user_cache}"
    mkdir -p ${spack_user_cache}

    # generate cmake cache file with uberenv and radiuss spack package
    run_section "spack_setup" "Spack setup and environment" "collapsed" \
      "Spack environment setup failed (Uberenv)" \
      ${uberenv_cmd} --setup-and-env-only --spec="${spec}" ${prefix_opt}

    if [[ -n ${ci_registry_token} ]]
    then
        run_section "registry_setup" "GitLab registry as Spack Buildcache" "collapsed" \
          "Adding gitlab registry to spack environment failed" \
          ${spack_cmd} -D ${spack_env_path} mirror add --unsigned --oci-username-variable ci_registry_user --oci-password-variable ci_registry_token gitlab_ci oci://${ci_registry_image}
    fi

    run_section "spack_build" "Spack build of dependencies" "collapsed" \
      "Spack build of dependencies failed (Uberenv)" \
      ${uberenv_cmd} --skip-setup-and-env --spec="${spec}" ${prefix_opt}

    if [[ -n ${ci_registry_token} && ${push_to_registry} == true ]]
    then
        run_section "buildcache_push" "Push dependencies to buildcache" "collapsed" \
          "Pushing dependencies to gitlab registry failed" \
          ${spack_cmd} -D ${spack_env_path} buildcache push --only dependencies gitlab_ci
    fi

    section_end
fi

###############################################################################
# HOST CONFIG / CMAKE CACHE FILE
###############################################################################
if [[ -z ${hostconfig} ]]
then
    # If no host config file was provided, we assume it was generated.
    # This means we are looking of a unique one in project dir.
    shopt -s nullglob; hostconfigs=( "${project_dir}"/*.cmake ); shopt -u nullglob
    if [[ ${#hostconfigs[@]} == 1 ]]
    then
        hostconfig_path=${hostconfigs[0]}
    elif [[ ${#hostconfigs[@]} == 0 ]]
    then
        print_error "No result for: ${project_dir}/*.cmake"
        print_error "Spack generated host-config not found."
        exit 1
    else
        print_error "More than one result for: ${project_dir}/*.cmake"
        print_error "${hostconfigs[@]}"
        print_error "Please specify one with HOST_CONFIG variable"
        exit 1
    fi
else
    # Using provided host-config file.
    hostconfig_path="${project_dir}/${hostconfig}"
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

#!/usr/bin/env bash

print_error ()
{
    local error_msg="${1}"
    echo -e "\e[31m[Error]: ${error_msg}\e[0m"
}

print_warning ()
{
    local warning_msg="${1}"
    echo -e "\e[1;30m[Warning]: ${warning_msg}\e[0m"
}

print_info ()
{
    local info_msg="${1}"
    echo -e "[Information]: ${info_msg}"
}

format_utc_timestamp ()
{
    local timestamp="${1}"
    if date -u -r "${timestamp}" "+%Y-%m-%d %H:%M:%S UTC" >/dev/null 2>&1
    then
        date -u -r "${timestamp}" "+%Y-%m-%d %H:%M:%S UTC"
    else
        date -u -d "@${timestamp}" "+%Y-%m-%d %H:%M:%S UTC"
    fi
}

format_elapsed_hms ()
{
    local elapsed="${1}"
    printf '%02d:%02d:%02d' $((elapsed / 3600)) $(((elapsed % 3600) / 60)) $((elapsed % 60))
}

script_start_time=${GITLAB_LOGS_SCRIPT_START_TIME:-$(date +%s)}
declare -A section_start_times
section_id_stack=()
section_counter=${GITLAB_LOGS_SECTION_COUNTER:-0}
section_indent=${GITLAB_LOGS_SECTION_INDENT:-""}

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

    section_counter=$((section_counter + 1))
    local section_id="${section_name}_${BASHPID}_${section_counter}"

    local timestamp=$(date +%s)
    local current_time=$(format_utc_timestamp "${timestamp}")
    local total_elapsed=$((timestamp - script_start_time))
    local total_elapsed_formatted=$(format_elapsed_hms "${total_elapsed}")

    section_start_times["${section_id}"]=${timestamp}
    section_id_stack+=("${section_id}")

    echo -e "\e[1;30m${section_indent}~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\e[0m"
    echo -e "\e[1;30m${section_indent}~ TIME                    | TOTAL    | SECTION  \e[0m"
    echo -e "\e[1;30m${section_indent}~ ${current_time} | ${total_elapsed_formatted} | ${section_title}\e[0m"
    echo -e "\e[0Ksection_start:${timestamp}:${section_id}[collapsed=${collapsed}]\r\e[0K${section_indent}~ ${section_title}"

    section_indent="${section_indent}  "
}

section_end ()
{
    if [[ ${#section_id_stack[@]} -eq 0 ]]; then
        print_warning "section_end called with empty stack"
        return 1
    fi

    section_indent="${section_indent%  }"

    local stack_index=$((${#section_id_stack[@]} - 1))
    local section_id="${section_id_stack[$stack_index]}"
    unset section_id_stack[$stack_index]

    local timestamp=$(date +%s)
    local current_time=$(format_utc_timestamp "${timestamp}")
    local total_elapsed=$((timestamp - script_start_time))
    local total_elapsed_formatted=$(format_elapsed_hms "${total_elapsed}")

    local section_start=${section_start_times["${section_id}"]:-${timestamp}}
    local section_elapsed=$((timestamp - section_start))
    local section_elapsed_formatted=$(format_elapsed_hms "${section_elapsed}")

    echo -e "\e[0Ksection_end:${timestamp}:${section_id}\r\e[0K\e[0m"
    echo -e "\e[1;30m${section_indent}~ ${current_time} | ${total_elapsed_formatted} | ${section_elapsed_formatted}\e[0m"
    echo -e "\e[1;30m${section_indent}~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\e[0m"

    unset "section_start_times[${section_id}]"
}

run_section ()
{
    local section_name="$1"
    local section_title="$2"
    local section_state="$3"
    local err_msg="$4"
    local status=0
    shift 4

    section_start "${section_name}" "${section_title}" "${section_state}"
    # Propagate logging context so nested scripts continue indent and total time.
    export GITLAB_LOGS_SCRIPT_START_TIME="${script_start_time}"
    export GITLAB_LOGS_SECTION_INDENT="${section_indent}"
    export GITLAB_LOGS_SECTION_COUNTER="${section_counter}"
    if "$@"; then
        section_end
    else
        status=$?
        section_end
        print_error "${err_msg}"
        exit ${status}
    fi
}

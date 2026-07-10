#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

# shellcheck disable=SC1091
. "${repo_dir}/scripts/pi_common.sh"

auto_mode=0
install_deps=ask
enable_service=ask
safe_mode=1
enable_credit_write=0
service_name="loki.service"
config_file="${LOKI_CONFIG_FILE:-}"
binary_override="${LOKI_BIN:-}"
smoke_timeout="${LOKI_FIRST_RUN_TIMEOUT:-10}"

prompt_yes_no() {
    local question="$1"
    local default_answer="$2"
    local reply

    if [ "${default_answer}" = "Y" ]; then
        printf '%s [Y/n] ' "${question}"
    else
        printf '%s [y/N] ' "${question}"
    fi

    read -r reply || true
    reply="${reply:-${default_answer}}"
    case "${reply}" in
        Y|y) return 0 ;;
        *) return 1 ;;
    esac
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --auto)
            auto_mode=1
            ;;
        --enable-service)
            enable_service=1
            ;;
        --safe-mode)
            safe_mode=1
            enable_credit_write=0
            ;;
        --allow-credit-write)
            safe_mode=0
            enable_credit_write=1
            ;;
        --skip-deps)
            install_deps=0
            ;;
        --config-file)
            shift
            config_file="${1:-}"
            ;;
        --loki-bin)
            shift
            binary_override="${1:-}"
            ;;
        --smoke-timeout)
            shift
            smoke_timeout="${1:-}"
            ;;
        *)
            loki_die "Unknown option: $1"
            ;;
    esac
    shift
done

if [ -z "${config_file}" ]; then
    config_file="$(loki_default_config_file)"
fi

loki_preflight "${repo_dir}" "${config_file}"

if [ "${auto_mode}" -eq 0 ]; then
    if [ "${install_deps}" = "ask" ]; then
        if prompt_yes_no "Install or update recommended packages first?" "Y"; then
            install_deps=1
        else
            install_deps=0
        fi
    fi

    if prompt_yes_no "Keep SAFE_MODE enabled (recommended)?" "Y"; then
        safe_mode=1
        enable_credit_write=0
    elif prompt_yes_no "Really enable unsafe credit-writing/changing behavior?" "N"; then
        safe_mode=0
        enable_credit_write=1
    else
        safe_mode=1
        enable_credit_write=0
    fi

    if [ "${enable_service}" = "ask" ]; then
        if loki_can_sudo && prompt_yes_no "Enable Loki to auto-start on boot after validation passes?" "Y"; then
            enable_service=1
        else
            enable_service=0
        fi
    fi
else
    if [ "${install_deps}" = "ask" ]; then
        install_deps=1
    fi
    if [ "${enable_service}" = "ask" ]; then
        enable_service=0
    fi
fi

if [ "${install_deps}" = "1" ]; then
    loki_install_dependencies
fi

loki_build_release "${repo_dir}"

if ! binary_path="$(loki_detect_binary "${repo_dir}" "${binary_override}")"; then
    loki_die "Could not find a built Loki executable. Set LOKI_BIN or pass --loki-bin with the full path."
fi

loki_write_config \
    "${config_file}" \
    "${repo_dir}" \
    "${binary_path}" \
    "${safe_mode}" \
    "${enable_credit_write}" \
    "${service_name}" \
    "system" \
    "${smoke_timeout}"

wrapper_dir="$(loki_default_wrapper_dir)"
loki_install_wrappers "${repo_dir}" "${wrapper_dir}"
loki_first_run_validation "${repo_dir}" "${config_file}" "${smoke_timeout}"

service_enabled=0
if [ "${enable_service}" = "1" ]; then
    if ! loki_can_sudo; then
        loki_warn "Skipping service install because this shell does not currently have sudo access."
    else
        loki_render_service_file "${repo_dir}" "${config_file}" "/etc/systemd/system/${service_name}" "${SUDO_USER:-${USER}}"
        loki_enable_service "${service_name}"
        service_enabled=1
    fi
fi

loki_print_next_steps "${repo_dir}" "${config_file}" "${service_enabled}" "${wrapper_dir}"

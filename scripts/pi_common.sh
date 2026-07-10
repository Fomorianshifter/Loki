#!/usr/bin/env bash

LOKI_MIN_FREE_KB=1048576

loki_log() {
    printf '[loki-pi] %s\n' "$*"
}

loki_warn() {
    printf '[loki-pi] WARNING: %s\n' "$*" >&2
}

loki_die() {
    printf '[loki-pi] ERROR: %s\n' "$*" >&2
    exit 1
}

loki_has_cmd() {
    command -v "$1" >/dev/null 2>&1
}

loki_can_sudo() {
    if [ "$(id -u)" -eq 0 ]; then
        return 0
    fi

    if loki_has_cmd sudo && sudo -n true >/dev/null 2>&1; then
        return 0
    fi

    return 1
}

loki_run_root() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    elif loki_has_cmd sudo; then
        sudo "$@"
    else
        loki_die "This step needs root access. Re-run with sudo or install sudo first."
    fi
}

loki_repo_root_from() {
    local origin_dir

    origin_dir="$(cd -- "$(dirname -- "$1")/.." && pwd -P)"
    printf '%s\n' "$origin_dir"
}

loki_default_config_file() {
    if loki_can_sudo; then
        printf '/etc/loki/loki.env\n'
    else
        printf '%s/.config/loki/loki.env\n' "${HOME}"
    fi
}

loki_default_wrapper_dir() {
    if loki_can_sudo; then
        printf '/usr/local/bin\n'
    else
        printf '%s/.local/bin\n' "${HOME}"
    fi
}

loki_detect_binary() {
    local repo_dir="$1"
    local explicit_bin="${2:-}"
    local candidate

    if [ -n "${explicit_bin}" ]; then
        if [ -x "${explicit_bin}" ]; then
            printf '%s\n' "${explicit_bin}"
            return 0
        fi

        loki_die "LOKI_BIN points to '${explicit_bin}', but that file is not executable."
    fi

    for candidate in \
        "${repo_dir}/build/release/loki_app" \
        "${repo_dir}/build/debug/loki_app" \
        "${repo_dir}/loki_app" \
        "${repo_dir}/build/release/loki" \
        "${repo_dir}/build/debug/loki" \
        "${repo_dir}/loki"; do
        if [ -x "${candidate}" ]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done

    return 1
}

loki_preflight() {
    local repo_dir="$1"
    local config_file="$2"
    local arch free_kb config_dir

    loki_log "Running preflight checks..."

    arch="$(uname -m)"
    case "${arch}" in
        armv6l|armv7l|aarch64)
            loki_log "Detected architecture: ${arch}"
            ;;
        *)
            loki_warn "Detected architecture '${arch}'. Raspberry Pi Zero 2 W installs normally report armv6l, armv7l, or aarch64."
            ;;
    esac

    free_kb="$(df -Pk "${repo_dir}" | awk 'NR==2 { print $4 }')"
    if [ -z "${free_kb}" ] || [ "${free_kb}" -lt "${LOKI_MIN_FREE_KB}" ]; then
        loki_die "At least 1 GB of free disk space is recommended. Free space at ${repo_dir} is too low."
    fi

    for cmd in awk sed make timeout; do
        if ! loki_has_cmd "${cmd}"; then
            loki_die "Missing required tool '${cmd}'. Install it with: sudo apt install -y ${cmd}"
        fi
    done

    if ! loki_has_cmd git; then
        loki_warn "git is not installed. In-place setup can continue, but clone/update mode will not work."
    fi

    if ! loki_has_cmd arm-linux-gnueabihf-gcc && ! loki_has_cmd gcc; then
        loki_die "No C compiler found. Install one with: sudo apt install -y build-essential or gcc-arm-linux-gnueabihf"
    fi

    if ! getent hosts github.com >/dev/null 2>&1; then
        loki_warn "github.com could not be resolved. Internet-based package installs or repo updates may fail."
    fi

    config_dir="$(dirname -- "${config_file}")"
    if ! mkdir -p "${config_dir}" 2>/dev/null; then
        if ! loki_can_sudo; then
            loki_die "Cannot create '${config_dir}'. Re-run with sudo or choose a writable config path."
        fi

        loki_run_root mkdir -p "${config_dir}"
    fi
}

loki_install_dependencies() {
    if ! loki_has_cmd apt-get; then
        loki_warn "apt-get is not available on this system, so dependencies were not auto-installed."
        return 0
    fi

    loki_log "Installing Raspberry Pi packages..."
    loki_run_root apt-get update
    loki_run_root apt-get install -y build-essential git make pkg-config
}

loki_build_release() {
    local repo_dir="$1"

    loki_log "Building Loki with make DEBUG=0..."
    (
        cd "${repo_dir}" || exit 1
        make clean >/dev/null 2>&1 || true
        make DEBUG=0
    )
}

loki_write_config() {
    local config_file="$1"
    local repo_dir="$2"
    local binary_path="$3"
    local safe_mode="$4"
    local enable_credit_write="$5"
    local service_name="$6"
    local scope="$7"
    local smoke_timeout="$8"
    local parent_dir

    parent_dir="$(dirname -- "${config_file}")"

    if ! mkdir -p "${parent_dir}" 2>/dev/null; then
        loki_run_root mkdir -p "${parent_dir}"
    fi

    if [ "$(id -u)" -eq 0 ] || [ -w "${parent_dir}" ]; then
        cat > "${config_file}" <<EOF
# Where the Loki checkout lives on this Pi.
LOKI_HOME=${repo_dir}

# Full path to the Loki executable that the service and helper commands start.
LOKI_BIN=${binary_path}

# The systemd unit name managed by lokictl and the loki-* shortcuts.
LOKI_SERVICE_NAME=${service_name}

# Which systemd scope to use. The setup scripts install a system-wide service by default.
LOKI_SYSTEMD_SCOPE=${scope}

# Seconds to allow the first-run smoke test before treating startup as healthy.
LOKI_FIRST_RUN_TIMEOUT=${smoke_timeout}

# Safe mode keeps destructive write-style behavior disabled by default.
SAFE_MODE=${safe_mode}

# Credit writing/changing is OFF by default. Only set this to 1 when SAFE_MODE is 0.
ENABLE_CREDIT_WRITE=${enable_credit_write}
EOF
    else
        loki_run_root tee "${config_file}" >/dev/null <<EOF
# Where the Loki checkout lives on this Pi.
LOKI_HOME=${repo_dir}

# Full path to the Loki executable that the service and helper commands start.
LOKI_BIN=${binary_path}

# The systemd unit name managed by lokictl and the loki-* shortcuts.
LOKI_SERVICE_NAME=${service_name}

# Which systemd scope to use. The setup scripts install a system-wide service by default.
LOKI_SYSTEMD_SCOPE=${scope}

# Seconds to allow the first-run smoke test before treating startup as healthy.
LOKI_FIRST_RUN_TIMEOUT=${smoke_timeout}

# Safe mode keeps destructive write-style behavior disabled by default.
SAFE_MODE=${safe_mode}

# Credit writing/changing is OFF by default. Only set this to 1 when SAFE_MODE is 0.
ENABLE_CREDIT_WRITE=${enable_credit_write}
EOF
    fi
}

loki_install_wrappers() {
    local repo_dir="$1"
    local bin_dir="$2"
    local link_target="${repo_dir}/scripts/lokictl"
    local wrapper

    if [ "$(id -u)" -eq 0 ] || \
       { [ -d "${bin_dir}" ] && [ -w "${bin_dir}" ]; } || \
       { [ ! -d "${bin_dir}" ] && [ -w "$(dirname -- "${bin_dir}")" ]; }; then
        mkdir -p "${bin_dir}"
        ln -sf "${link_target}" "${bin_dir}/lokictl"
        for wrapper in loki-start loki-stop loki-status loki-logs; do
            ln -sf "${link_target}" "${bin_dir}/${wrapper}"
        done
    else
        loki_run_root mkdir -p "${bin_dir}"
        loki_run_root ln -sf "${link_target}" "${bin_dir}/lokictl"
        for wrapper in loki-start loki-stop loki-status loki-logs; do
            loki_run_root ln -sf "${link_target}" "${bin_dir}/${wrapper}"
        done
    fi
}

loki_first_run_validation() {
    local repo_dir="$1"
    local config_file="$2"
    local smoke_timeout="$3"
    local output status

    loki_log "Running a ${smoke_timeout}-second first-run smoke test..."

    if output="$(cd "${repo_dir}" && LOKI_CONFIG_FILE="${config_file}" timeout "${smoke_timeout}" "${repo_dir}/scripts/loki-run" 2>&1)"; then
        loki_log "Smoke test exited cleanly."
    else
        status="$?"
        case "${status}" in
            124)
                loki_log "Smoke test timed out as expected, which means Loki stayed alive long enough to pass startup validation."
                return 0
                ;;
            *)
                printf '%s\n' "${output}" >&2
                loki_die "The first-run smoke test failed. Review the output above, then retry after fixing the hardware or configuration issue."
                ;;
        esac
    fi
}

loki_render_service_file() {
    local repo_dir="$1"
    local config_file="$2"
    local destination="$3"
    local run_as_user="$4"
    local template_file="${repo_dir}/packaging/systemd/loki.service"
    local escaped_repo escaped_config escaped_runner escaped_user

    escaped_repo="$(printf '%s' "${repo_dir}" | sed 's/[&|]/\\&/g')"
    escaped_config="$(printf '%s' "${config_file}" | sed 's/[&|]/\\&/g')"
    escaped_runner="$(printf '%s' "${repo_dir}/scripts/loki-run" | sed 's/[&|]/\\&/g')"
    escaped_user="$(printf '%s' "${run_as_user}" | sed 's/[&|]/\\&/g')"

    sed \
        -e "s|__LOKI_WORKDIR__|${escaped_repo}|g" \
        -e "s|__LOKI_ENV_FILE__|${escaped_config}|g" \
        -e "s|__LOKI_RUNNER__|${escaped_runner}|g" \
        -e "s|__LOKI_USER__|${escaped_user}|g" \
        "${template_file}" | loki_run_root tee "${destination}" >/dev/null
}

loki_enable_service() {
    local service_name="$1"

    loki_log "Enabling ${service_name} for automatic start..."
    loki_run_root systemctl daemon-reload
    loki_run_root systemctl enable --now "${service_name}"
}

loki_print_next_steps() {
    local repo_dir="$1"
    local config_file="$2"
    local service_enabled="$3"
    local wrapper_dir="$4"

    printf '\n'
    loki_log "Setup complete."
    printf 'Config file: %s\n' "${config_file}"
    printf 'Helper commands: %s/loki-start, %s/loki-stop, %s/loki-status, %s/loki-logs\n' \
        "${wrapper_dir}" "${wrapper_dir}" "${wrapper_dir}" "${wrapper_dir}"
    printf 'Manual run: LOKI_CONFIG_FILE=%s %s/scripts/loki-run\n' "${config_file}" "${repo_dir}"
    if [ "${service_enabled}" = "1" ]; then
        printf 'Autostart: enabled (use "loki-status" or "sudo systemctl status loki.service").\n'
    else
        printf 'Autostart: not enabled. You can enable it later with "sudo systemctl enable --now loki.service".\n'
    fi
    printf 'Troubleshooting: loki-logs, journalctl -u loki.service -n 100 --no-pager, or inspect %s\n' "${config_file}"
}

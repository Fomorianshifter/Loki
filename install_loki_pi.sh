#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]:-$0}")" && pwd -P)"
repo_dir="${script_dir}"
repo_url="https://github.com/Fomorianshifter/Loki.git"
install_dir="${HOME}/Loki"
enable_service=0
allow_credit_write=0
extra_args=()

while [ "$#" -gt 0 ]; do
    case "$1" in
        --auto)
            ;;
        --enable-service)
            enable_service=1
            ;;
        --safe-mode)
            allow_credit_write=0
            ;;
        --allow-credit-write)
            allow_credit_write=1
            ;;
        --install-dir)
            shift
            install_dir="${1:-}"
            ;;
        --repo-url)
            shift
            repo_url="${1:-}"
            ;;
        --config-file|--loki-bin|--smoke-timeout)
            extra_args+=("$1")
            shift
            extra_args+=("${1:-}")
            ;;
        *)
            extra_args+=("$1")
            ;;
    esac
    shift
done

if [ ! -f "${repo_dir}/scripts/pi_common.sh" ] || [ ! -f "${repo_dir}/Makefile" ]; then
    if ! command -v git >/dev/null 2>&1; then
        printf 'git is required to clone Loki. Install it first with: sudo apt install -y git\n' >&2
        exit 1
    fi

    if [ -d "${install_dir}/.git" ]; then
        printf '[loki-pi] Updating existing checkout at %s\n' "${install_dir}"
        git -C "${install_dir}" pull --ff-only
    else
        printf '[loki-pi] Cloning %s into %s\n' "${repo_url}" "${install_dir}"
        git clone "${repo_url}" "${install_dir}"
    fi

    bootstrap_args=(--auto)
    if [ "${enable_service}" -eq 1 ]; then
        bootstrap_args+=(--enable-service)
    fi
    if [ "${allow_credit_write}" -eq 1 ]; then
        bootstrap_args+=(--allow-credit-write)
    else
        bootstrap_args+=(--safe-mode)
    fi
    bootstrap_args+=("${extra_args[@]}")

    exec "${install_dir}/install_loki_pi.sh" "${bootstrap_args[@]}"
fi

setup_args=(--auto)
if [ "${enable_service}" -eq 1 ]; then
    setup_args+=(--enable-service)
fi
if [ "${allow_credit_write}" -eq 1 ]; then
    setup_args+=(--allow-credit-write)
else
    setup_args+=(--safe-mode)
fi
setup_args+=("${extra_args[@]}")

exec "${repo_dir}/setup_pi.sh" "${setup_args[@]}"

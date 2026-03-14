#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Loki local tool helper

Usage:
  loki-local-tool run -- <command...>
  loki-local-tool nmap <target> [ports]
  loki-local-tool quick-scan <target>

This helper is intentionally local-only. It is not called by the web UI.
Use it on the device shell when you need power-user commands that should not
be exposed through Loki's network-facing control surface.
EOF
}

if [[ $# -lt 1 ]]; then
    usage
    exit 1
fi

case "$1" in
    run)
        shift
        if [[ "${1:-}" != "--" || $# -lt 2 ]]; then
            echo "[ERROR] Use: loki-local-tool run -- <command...>"
            exit 1
        fi
        shift
        echo "[INFO] Executing local-only command: $*"
        exec "$@"
        ;;
    nmap)
        if [[ $# -lt 2 ]]; then
            echo "[ERROR] Use: loki-local-tool nmap <target> [ports]"
            exit 1
        fi
        target="$2"
        ports="${3:-}"
        if [[ -n "${ports}" ]]; then
            exec nmap -Pn -sV -p "${ports}" -- "${target}"
        fi
        exec nmap -sn -- "${target}"
        ;;
    quick-scan)
        if [[ $# -ne 2 ]]; then
            echo "[ERROR] Use: loki-local-tool quick-scan <target>"
            exit 1
        fi
        exec nmap -Pn -T4 --top-ports 20 -- "$2"
        ;;
    -h|--help|help)
        usage
        ;;
    *)
        echo "[ERROR] Unknown subcommand: $1"
        usage
        exit 1
        ;;
esac
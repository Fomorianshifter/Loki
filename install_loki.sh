#!/usr/bin/env bash

set -euo pipefail

REPO_URL="https://github.com/Fomorianshifter/Loki.git"
INSTALL_DIR="/opt/loki"
SERVICE_PATH="/etc/systemd/system/loki.service"
INSTALL_MODE="install"
PROMPT_FOR_REBOOT=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --update)
            INSTALL_MODE="update"
            shift
            ;;
        --no-reboot-prompt)
            PROMPT_FOR_REBOOT=0
            shift
            ;;
        *)
            echo "[ERROR] Unknown installer argument: $1"
            exit 1
            ;;
    esac
done

if [[ "${LOKI_SKIP_REBOOT_PROMPT:-0}" == "1" ]]; then
    PROMPT_FOR_REBOOT=0
fi

if [[ ${EUID} -ne 0 ]]; then
    echo "[ERROR] Please run this installer with sudo or as root."
    exit 1
fi

CURRENT_USER="${SUDO_USER:-$(logname 2>/dev/null || echo root)}"

print_header() {
    echo "================================================"
    echo "Loki Installer for Raspberry Pi"
    echo "================================================"
    echo
}

detect_platform() {
    if [[ ! -f /etc/os-release ]]; then
        echo "[ERROR] Unsupported Linux distribution. Loki installer expects Raspberry Pi OS or Debian-based Linux."
        exit 1
    fi

    . /etc/os-release
    ARCH="$(uname -m)"

    echo "[INFO] Detected OS: ${PRETTY_NAME:-unknown}"
    echo "[INFO] Detected architecture: ${ARCH}"

    case "${ARCH}" in
        armv7l|armv6l|aarch64)
            ;;
        *)
            echo "[WARN] This installer is optimized for Raspberry Pi ARM systems. Continuing anyway."
            ;;
    esac

    echo
}

install_packages() {
    echo "[INFO] Installing packages..."
    apt-get update
    apt-get install -y \
        avahi-daemon \
        build-essential \
        git \
        i2c-tools \
        make \
        pkg-config
    echo "[OK] Packages installed"
    echo
}

enable_interfaces() {
    echo "[INFO] Enabling Raspberry Pi interfaces..."

    if command -v raspi-config >/dev/null 2>&1; then
        raspi-config nonint do_ssh 0 || true
        raspi-config nonint do_spi 0 || true
        raspi-config nonint do_i2c 0 || true
        raspi-config nonint do_serial_cons 1 || true
        raspi-config nonint do_serial_hw 0 || true
        echo "[OK] Requested SSH, SPI, I2C, and serial configuration through raspi-config"
    else
        echo "[WARN] raspi-config not found; please manually verify SSH, SPI, I2C, and serial settings"
    fi

    echo
}

sync_repository() {
    echo "[INFO] Syncing Loki source into ${INSTALL_DIR}..."

    if [[ -d "${INSTALL_DIR}/.git" ]]; then
        if [[ "${INSTALL_MODE}" == "update" ]]; then
            git -C "${INSTALL_DIR}" fetch --all --tags
            git -C "${INSTALL_DIR}" pull --ff-only
        else
            echo "[INFO] Using existing local checkout in ${INSTALL_DIR}"
            echo "[INFO] Pass --update if you want the installer to pull from ${REPO_URL}"
        fi
    elif [[ -d "${INSTALL_DIR}" ]]; then
        echo "[INFO] Using existing local source tree in ${INSTALL_DIR}"
    else
        rm -rf "${INSTALL_DIR}"
        git clone "${REPO_URL}" "${INSTALL_DIR}"
    fi

    if [[ -f "${INSTALL_DIR}/loki.conf" ]]; then
        cp "${INSTALL_DIR}/loki.conf" "${INSTALL_DIR}/loki.conf.bak"
    fi

    chown -R "${CURRENT_USER}:${CURRENT_USER}" "${INSTALL_DIR}"
    echo "[OK] Repository ready"
    echo
}

build_loki() {
    echo "[INFO] Building Loki..."
    cd "${INSTALL_DIR}"
    sudo -u "${CURRENT_USER}" chmod +x ./build_native.sh
    sudo -u "${CURRENT_USER}" ./build_native.sh release
    echo "[OK] Loki build complete"
    echo
}

install_service() {
    echo "[INFO] Installing systemd service..."

    cat > "${SERVICE_PATH}" <<EOF
[Unit]
Description=Loki Embedded Companion
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=${INSTALL_DIR}
ExecStart=${INSTALL_DIR}/build/release/loki_app
Restart=on-failure
RestartSec=5
User=root
Environment=HOME=/root

[Install]
WantedBy=multi-user.target
EOF

    systemctl daemon-reload
    systemctl enable loki
    systemctl restart loki

    echo "[OK] Loki service installed and started"
    echo
}

print_summary() {
    echo "[OK] Loki installation complete"
    echo
    echo "Next checks:"
    echo "  sudo systemctl status loki"
    echo "  sudo journalctl -u loki -f"
    echo "  ls /dev/spidev0.0"
    echo
    echo "Config file: ${INSTALL_DIR}/loki.conf"
    echo "Service file: ${SERVICE_PATH}"
    echo
}

offer_reboot() {
    if [[ ${PROMPT_FOR_REBOOT} -eq 0 ]]; then
        echo "[INFO] Reboot prompt suppressed"
        echo "[INFO] Reboot manually after first boot if SPI, I2C, or serial settings changed."
        return
    fi

    printf "Reboot now to ensure interface changes are active? [y/N]: "
    read -r answer
    case "${answer}" in
        y|Y|yes|YES)
            reboot
            ;;
        *)
            echo "[INFO] Reboot skipped. If SPI/I2C/serial were just enabled, reboot before running Loki on hardware."
            ;;
    esac
}

run_install() {
    print_header
    detect_platform
    install_packages
    enable_interfaces
    sync_repository
    build_loki
    install_service
    print_summary
    offer_reboot
}

run_install
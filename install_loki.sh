#!/usr/bin/env bash

set -euo pipefail

REPO_URL="https://github.com/Fomorianshifter/Loki.git"
INSTALL_DIR="/opt/loki"
SERVICE_PATH="/etc/systemd/system/loki.service"
INSTALL_MODE="install"
PROMPT_FOR_REBOOT=1
INTERACTIVE_MENU=1
DISPLAY_BACKEND="auto"
FRAMEBUFFER_DEVICE="/dev/fb1"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --auto)
            INSTALL_MODE="install"
            INTERACTIVE_MENU=0
            shift
            ;;
        --update)
            INSTALL_MODE="update"
            INTERACTIVE_MENU=0
            shift
            ;;
        --no-reboot-prompt)
            PROMPT_FOR_REBOOT=0
            INTERACTIVE_MENU=0
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

if [[ "${DEBIAN_FRONTEND:-}" == "noninteractive" ]]; then
    INTERACTIVE_MENU=0
fi

if [[ ${EUID} -ne 0 ]]; then
    echo "[ERROR] Please run this installer with sudo or as root."
    exit 1
fi

CURRENT_USER="${SUDO_USER:-$(logname 2>/dev/null || echo root)}"
CURRENT_USER_HOME="$(eval echo "~${CURRENT_USER}")"

print_header() {
    echo "================================================"
    echo "Loki Installer for Raspberry Pi"
    echo "================================================"
    echo
}

choose_install_mode() {
    if [[ ${INTERACTIVE_MENU} -eq 0 || ! -t 0 ]]; then
        return
    fi

    echo "Choose an option:"
    echo "  1) Automatic installation"
    echo "  2) Update existing Loki installation"
    echo "  3) Exit"
    echo
    printf "Selection [1-3]: "
    read -r selection

    case "${selection}" in
        ""|1)
            INSTALL_MODE="install"
            ;;
        2)
            INSTALL_MODE="update"
            ;;
        3)
            echo "[INFO] Installer exited"
            exit 0
            ;;
        *)
            echo "[ERROR] Invalid selection: ${selection}"
            exit 1
            ;;
    esac

    echo
}

choose_display_type() {
    if [[ ${INTERACTIVE_MENU} -eq 0 || ! -t 0 ]]; then
        return
    fi

    echo "Select your display type for Loki:"
    echo "  1) TFT (SPI framebuffer, usually /dev/fb1)"
    echo "  2) LCD (SPI framebuffer, usually /dev/fb1)"
    echo "  3) OLED (framebuffer device, often /dev/fb0)"
    echo "  4) EPD / ePaper (framebuffer device, often /dev/fb0)"
    echo "  5) Auto / use existing framebuffer settings"
    echo
    printf "Selection [1-5]: "
    read -r display_choice

    case "${display_choice}" in
        ""|5)
            DISPLAY_BACKEND="auto"
            FRAMEBUFFER_DEVICE="/dev/fb1"
            ;;
        1)
            DISPLAY_BACKEND="framebuffer"
            FRAMEBUFFER_DEVICE="/dev/fb1"
            ;;
        2)
            DISPLAY_BACKEND="framebuffer"
            FRAMEBUFFER_DEVICE="/dev/fb1"
            ;;
        3)
            DISPLAY_BACKEND="framebuffer"
            FRAMEBUFFER_DEVICE="/dev/fb0"
            ;;
        4)
            DISPLAY_BACKEND="framebuffer"
            FRAMEBUFFER_DEVICE="/dev/fb0"
            ;;
        *)
            echo "[WARN] Invalid selection, defaulting to auto framebuffer settings."
            DISPLAY_BACKEND="auto"
            FRAMEBUFFER_DEVICE="/dev/fb1"
            ;;
    esac

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
    systemctl enable avahi-daemon >/dev/null 2>&1 || true
    systemctl restart avahi-daemon >/dev/null 2>&1 || true
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

save_display_config() {
    if [[ -f "${INSTALL_DIR}/loki.conf" ]]; then
        sed -i -e 's/^backend=.*/backend='"${DISPLAY_BACKEND}"'/' \
               -e 's|^framebuffer_device=.*|framebuffer_device='"${FRAMEBUFFER_DEVICE}"'|' \
               "${INSTALL_DIR}/loki.conf"
        echo "[OK] Display configuration saved: ${DISPLAY_BACKEND} ${FRAMEBUFFER_DEVICE}"
        echo
    fi
}

install_external_tools() {
    if [[ ${INTERACTIVE_MENU} -eq 0 || ! -t 0 ]]; then
        return
    fi

    echo "Would you like to run external install scripts for Bjorn, Ragnar, or Pwnagotchi if they exist?"
    printf "Run external install scripts? [y/N]: "
    read -r answer

    case "${answer}" in
        y|Y|yes|YES)
            echo "[INFO] Checking for external tool installers..."
            ;;
        *)
            echo "[INFO] Skipping external tool installation."
            return
            ;;
    esac

    local install_paths=(
        "/opt/bjorn/install.sh"
        "/opt/ragnar/install.sh"
        "/opt/pwnagotchi/install.sh"
        "/root/bjorn/install.sh"
        "/root/ragnar/install.sh"
        "/root/pwnagotchi/install.sh"
        "${CURRENT_USER_HOME}/bjorn/install.sh"
        "${CURRENT_USER_HOME}/ragnar/install.sh"
        "${CURRENT_USER_HOME}/pwnagotchi/install.sh"
    )

    for tool_script in "${install_paths[@]}"; do
        if [[ -f "${tool_script}" ]]; then
            echo "[INFO] Found external installer: ${tool_script}"
            if bash "${tool_script}"; then
                echo "[OK] External installer completed: ${tool_script}"
            else
                echo "[WARN] External installer failed: ${tool_script}"
            fi
        fi
    done

    echo "[INFO] External tool install step finished."
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

prepare_runtime_state() {
    echo "[INFO] Preparing runtime state..."
    mkdir -p /var/lib/loki
    chmod 755 /var/lib/loki
    if [[ -f "${INSTALL_DIR}/loki_local_tool.sh" ]]; then
        install -m 0755 "${INSTALL_DIR}/loki_local_tool.sh" /usr/local/bin/loki-local-tool
    fi
    echo "[OK] Runtime state directory ready"
    echo
}

install_service() {
    echo "[INFO] Installing systemd service..."

    cat > "${SERVICE_PATH}" <<EOF
[Unit]
Description=Loki Embedded Companion
After=local-fs.target

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
    local host_name
    local host_label
    local primary_ip

    host_name="$(hostname 2>/dev/null || echo loki)"
    host_label="${host_name}.local"
    primary_ip="$(hostname -I 2>/dev/null | awk '{print $1}')"

    echo "[OK] Loki installation complete"
    echo
    echo "Next checks:"
    echo "  sudo systemctl status loki"
    echo "  sudo journalctl -u loki -f"
    echo "  ls /dev/spidev0.0"
    echo
    echo "Open the web UI:"
    echo "  http://127.0.0.1:8080"
    echo "  http://${host_label}:8080"
    if [[ -n "${primary_ip}" ]]; then
        echo "  http://${primary_ip}:8080"
    fi
    echo "API health check:"
    echo "  curl http://127.0.0.1:8080/api/status"
    echo
    echo "Local-only power tool helper: /usr/local/bin/loki-local-tool"
    echo "Advanced mode flag path: /boot/loki-advanced-mode.flag"
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
    choose_install_mode
    choose_display_type
    detect_platform
    install_packages
    enable_interfaces
    sync_repository
    save_display_config
    build_loki
    prepare_runtime_state
    install_external_tools
    install_service
    print_summary
    offer_reboot
}

run_install
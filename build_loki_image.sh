#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="${SCRIPT_DIR}/dist/image-build"
OUTPUT_DIR="${SCRIPT_DIR}/dist"
DEFAULT_IMAGE_NAME="loki-rpi-bookworm-armhf.img"
BASE_IMAGE=""
OUTPUT_IMAGE="${OUTPUT_DIR}/${DEFAULT_IMAGE_NAME}"
ENABLE_SSH=1
KEEP_WORKDIR=0
HOSTNAME="loki"

usage() {
    cat <<'EOF'
Usage: sudo ./build_loki_image.sh --base-image <path-or-url> [options]

Builds a Raspberry Pi OS image with Loki staged for first boot.
The generated output is a compressed .img.xz file in ./dist.

Required:
  --base-image <path-or-url>   Local .img/.img.xz/.zip path or downloadable URL

Optional:
  --output <path>              Output .img path before compression
  --hostname <name>            Target hostname written to the image (default: loki)
  --disable-ssh                Do not enable SSH on first boot
  --keep-workdir               Preserve temporary mount and extraction files
  --help                       Show this help text

Examples:
  sudo ./build_loki_image.sh \
    --base-image ~/Downloads/raspios-bookworm-armhf-lite.img.xz

  sudo ./build_loki_image.sh \
    --base-image https://downloads.raspberrypi.com/raspios_lite_armhf_latest \
    --output ./dist/loki-custom.img \
    --hostname loki-zero
EOF
}

log_info() {
    echo "[INFO] $*"
}

log_ok() {
    echo "[OK] $*"
}

log_warn() {
    echo "[WARN] $*"
}

die() {
    echo "[ERROR] $*" >&2
    exit 1
}

require_root() {
    if [[ ${EUID} -ne 0 ]]; then
        die "Run this script with sudo or as root so it can mount and modify the image."
    fi
}

require_command() {
    local command_name="$1"
    command -v "${command_name}" >/dev/null 2>&1 || die "Missing required command: ${command_name}"
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --base-image)
                BASE_IMAGE="${2:-}"
                shift 2
                ;;
            --output)
                OUTPUT_IMAGE="${2:-}"
                shift 2
                ;;
            --hostname)
                HOSTNAME="${2:-}"
                shift 2
                ;;
            --disable-ssh)
                ENABLE_SSH=0
                shift
                ;;
            --keep-workdir)
                KEEP_WORKDIR=1
                shift
                ;;
            --help|-h)
                usage
                exit 0
                ;;
            *)
                die "Unknown argument: $1"
                ;;
        esac
    done

    [[ -n "${BASE_IMAGE}" ]] || die "--base-image is required"
}

ensure_dependencies() {
    require_command file
    require_command tar
    require_command losetup
    require_command mount
    require_command umount
    require_command findmnt
    require_command xz

    if [[ "${BASE_IMAGE}" =~ ^https?:// ]]; then
        if command -v curl >/dev/null 2>&1; then
            DOWNLOAD_TOOL="curl"
        elif command -v wget >/dev/null 2>&1; then
            DOWNLOAD_TOOL="wget"
        else
            die "Need curl or wget to download the base image URL"
        fi
    else
        DOWNLOAD_TOOL=""
    fi
}

prepare_workspace() {
    mkdir -p "${WORK_DIR}" "${OUTPUT_DIR}" "$(dirname "${OUTPUT_IMAGE}")"

    SOURCE_TARBALL="${WORK_DIR}/loki-src.tar.gz"
    RAW_IMAGE_INPUT="${WORK_DIR}/base-image"
    IMAGE_TO_MODIFY="${WORK_DIR}/$(basename "${OUTPUT_IMAGE}")"
    BOOT_MOUNT="${WORK_DIR}/mnt-boot"
    ROOT_MOUNT="${WORK_DIR}/mnt-root"

    rm -rf "${BOOT_MOUNT}" "${ROOT_MOUNT}"
    mkdir -p "${BOOT_MOUNT}" "${ROOT_MOUNT}"
}

acquire_base_image() {
    local source_name

    source_name="$(basename "${BASE_IMAGE}")"
    RAW_IMAGE_INPUT="${WORK_DIR}/${source_name}"

    if [[ -f "${BASE_IMAGE}" ]]; then
        log_info "Copying base image from local path"
        cp "${BASE_IMAGE}" "${RAW_IMAGE_INPUT}"
    elif [[ "${BASE_IMAGE}" =~ ^https?:// ]]; then
        log_info "Downloading base image"
        if [[ "${DOWNLOAD_TOOL}" == "curl" ]]; then
            curl -L "${BASE_IMAGE}" -o "${RAW_IMAGE_INPUT}"
        else
            wget -O "${RAW_IMAGE_INPUT}" "${BASE_IMAGE}"
        fi
    else
        die "Base image not found: ${BASE_IMAGE}"
    fi
}

extract_base_image() {
    local detected_type

    detected_type="$(file -b "${RAW_IMAGE_INPUT}")"

    case "${RAW_IMAGE_INPUT}" in
        *.img)
            cp "${RAW_IMAGE_INPUT}" "${IMAGE_TO_MODIFY}"
            ;;
        *.img.xz|*.xz)
            log_info "Decompressing xz image"
            xz -dc "${RAW_IMAGE_INPUT}" > "${IMAGE_TO_MODIFY}"
            ;;
        *.zip)
            require_command unzip
            log_info "Extracting zip image"
            unzip -p "${RAW_IMAGE_INPUT}" '*.img' > "${IMAGE_TO_MODIFY}"
            ;;
        *)
            if printf '%s' "${detected_type}" | grep -qi 'XZ compressed data'; then
                log_info "Detected xz-compressed image"
                xz -dc "${RAW_IMAGE_INPUT}" > "${IMAGE_TO_MODIFY}"
            elif printf '%s' "${detected_type}" | grep -qi 'Zip archive data'; then
                require_command unzip
                log_info "Detected zip-compressed image"
                unzip -p "${RAW_IMAGE_INPUT}" '*.img' > "${IMAGE_TO_MODIFY}"
            elif printf '%s' "${detected_type}" | grep -qi 'DOS/MBR boot sector\|Linux rev 1.0 ext'; then
                log_info "Detected raw disk image"
                cp "${RAW_IMAGE_INPUT}" "${IMAGE_TO_MODIFY}"
            else
                die "Unsupported base image format: ${RAW_IMAGE_INPUT} (${detected_type})"
            fi
            ;;
    esac

    [[ -f "${IMAGE_TO_MODIFY}" ]] || die "Failed to prepare raw image"
}

create_source_bundle() {
    log_info "Bundling current Loki repository for first boot"

    tar \
        --exclude='.git' \
        --exclude='.github' \
        --exclude='build' \
        --exclude='dist' \
        --exclude='Loki_release.zip' \
        --exclude='*.img' \
        --exclude='*.img.xz' \
        --exclude='*.tar.gz' \
        -czf "${SOURCE_TARBALL}" \
        -C "${SCRIPT_DIR}" \
        .

    log_ok "Source bundle created at ${SOURCE_TARBALL}"
}

cleanup() {
    local exit_code=$?

    if [[ -n "${BOOT_MOUNT:-}" ]] && findmnt -rn "${BOOT_MOUNT}" >/dev/null 2>&1; then
        umount "${BOOT_MOUNT}" || true
    fi
    if [[ -n "${ROOT_MOUNT:-}" ]] && findmnt -rn "${ROOT_MOUNT}" >/dev/null 2>&1; then
        umount "${ROOT_MOUNT}" || true
    fi
    if [[ -n "${LOOP_DEVICE:-}" ]]; then
        losetup -d "${LOOP_DEVICE}" || true
    fi

    if [[ ${KEEP_WORKDIR} -eq 0 ]]; then
        rm -rf "${BOOT_MOUNT:-}" "${ROOT_MOUNT:-}"
    else
        log_warn "Keeping work directory at ${WORK_DIR}"
    fi

    return ${exit_code}
}

mount_image() {
    trap cleanup EXIT

    log_info "Attaching loop device"
    LOOP_DEVICE="$(losetup --find --show --partscan "${IMAGE_TO_MODIFY}")"
    BOOT_PARTITION="${LOOP_DEVICE}p1"
    ROOT_PARTITION="${LOOP_DEVICE}p2"

    [[ -b "${BOOT_PARTITION}" ]] || die "Boot partition not found on ${LOOP_DEVICE}"
    [[ -b "${ROOT_PARTITION}" ]] || die "Root partition not found on ${LOOP_DEVICE}"

    mount "${BOOT_PARTITION}" "${BOOT_MOUNT}"
    mount "${ROOT_PARTITION}" "${ROOT_MOUNT}"
}

write_firstboot_assets() {
    local boot_config_dir
    local boot_ssh_path

    log_info "Staging Loki bundle into the image"

    mkdir -p "${ROOT_MOUNT}/opt/loki-image"
    cp "${SOURCE_TARBALL}" "${ROOT_MOUNT}/opt/loki-image/loki-src.tar.gz"

    mkdir -p "${ROOT_MOUNT}/usr/local/sbin"
    cat > "${ROOT_MOUNT}/usr/local/sbin/loki-firstboot.sh" <<EOF
#!/usr/bin/env bash

set -euo pipefail

LOG_FILE="/var/log/loki-firstboot.log"
exec > >(tee -a "\${LOG_FILE}") 2>&1

echo "[INFO] Loki first-boot provisioning started"

mkdir -p /opt/loki
tar -xzf /opt/loki-image/loki-src.tar.gz -C /opt/loki --strip-components=1
chmod +x /opt/loki/install_loki.sh

echo "${HOSTNAME}" >/etc/hostname
if grep -q '^127.0.1.1' /etc/hosts; then
    sed -i "s/^127.0.1.1.*/127.0.1.1\t${HOSTNAME}/" /etc/hosts
else
    echo -e "127.0.1.1\t${HOSTNAME}" >>/etc/hosts
fi

cd /opt/loki
LOKI_SKIP_REBOOT_PROMPT=1 DEBIAN_FRONTEND=noninteractive /opt/loki/install_loki.sh --no-reboot-prompt

systemctl disable loki-firstboot.service || true
rm -f /etc/systemd/system/loki-firstboot.service
rm -f /etc/systemd/system/multi-user.target.wants/loki-firstboot.service
rm -f /opt/loki-image/loki-src.tar.gz
systemctl daemon-reload || true

echo "[OK] Loki first-boot provisioning completed"
EOF
    chmod +x "${ROOT_MOUNT}/usr/local/sbin/loki-firstboot.sh"

    mkdir -p "${ROOT_MOUNT}/etc/systemd/system/multi-user.target.wants"
    cat > "${ROOT_MOUNT}/etc/systemd/system/loki-firstboot.service" <<'EOF'
[Unit]
Description=Loki first boot provisioning
After=network-online.target
Wants=network-online.target
ConditionPathExists=/opt/loki-image/loki-src.tar.gz

[Service]
Type=oneshot
ExecStart=/usr/local/sbin/loki-firstboot.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

    ln -sf ../loki-firstboot.service \
        "${ROOT_MOUNT}/etc/systemd/system/multi-user.target.wants/loki-firstboot.service"

    echo "${HOSTNAME}" > "${ROOT_MOUNT}/etc/hostname"

    if [[ ${ENABLE_SSH} -eq 1 ]]; then
        if [[ -d "${BOOT_MOUNT}/firmware" ]]; then
            boot_config_dir="${BOOT_MOUNT}/firmware"
        else
            boot_config_dir="${BOOT_MOUNT}"
        fi

        boot_ssh_path="${boot_config_dir}/ssh"
        : > "${boot_ssh_path}"
        log_ok "SSH enabled for first boot"
    fi

    if [[ -d "${BOOT_MOUNT}/firmware" ]]; then
        cat > "${BOOT_MOUNT}/firmware/loki-image.txt" <<EOF
Loki image customization
- Hostname: ${HOSTNAME}
- SSH enabled: $([[ ${ENABLE_SSH} -eq 1 ]] && echo yes || echo no)
- First boot service: loki-firstboot.service
EOF
    else
        cat > "${BOOT_MOUNT}/loki-image.txt" <<EOF
Loki image customization
- Hostname: ${HOSTNAME}
- SSH enabled: $([[ ${ENABLE_SSH} -eq 1 ]] && echo yes || echo no)
- First boot service: loki-firstboot.service
EOF
    fi
}

finalize_image() {
    local compressed_output

    sync
    umount "${BOOT_MOUNT}"
    umount "${ROOT_MOUNT}"
    losetup -d "${LOOP_DEVICE}"
    LOOP_DEVICE=""

    mkdir -p "$(dirname "${OUTPUT_IMAGE}")"
    mv "${IMAGE_TO_MODIFY}" "${OUTPUT_IMAGE}"

    compressed_output="${OUTPUT_IMAGE}.xz"
    log_info "Compressing image"
    xz -T0 -f "${OUTPUT_IMAGE}"

    log_ok "Burnable image created: ${compressed_output}"
}

main() {
    parse_args "$@"
    require_root
    ensure_dependencies
    prepare_workspace
    acquire_base_image
    extract_base_image
    create_source_bundle
    mount_image
    write_firstboot_assets
    finalize_image
}

main "$@"
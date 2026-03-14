#!/usr/bin/env bash

set -euo pipefail

MODE="${1:-release}"
BUILD_DIR="build/release"
OPT_FLAGS=(-O3 -DDEBUG=0 -DLOG_LEVEL=2)
ARCH_FLAGS=()

case "$(uname -m)" in
    armv6l|armv7l|armv8l)
        ARCH_FLAGS=(-mcpu=cortex-a53)
        ;;
    aarch64)
        ARCH_FLAGS=(-mcpu=cortex-a53)
        ;;
    *)
        ARCH_FLAGS=()
        ;;
esac

if [[ "${MODE}" == "debug" ]]; then
    BUILD_DIR="build/debug"
    OPT_FLAGS=(-g -O0 -DDEBUG -DLOG_LEVEL=4)
elif [[ "${MODE}" != "release" ]]; then
    echo "Usage: ./build_native.sh [release|debug]"
    exit 1
fi

mkdir -p "${BUILD_DIR}"

echo "[INFO] Building Loki (${MODE})"

gcc \
    -Wall -Wextra \
    "${ARCH_FLAGS[@]}" \
    -I. \
    -DPLATFORM=PLATFORM_LINUX \
    "${OPT_FLAGS[@]}" \
    eeprom_driver.c \
    flash_driver.c \
    flipper_uart.c \
    gpio.c \
    i2c.c \
    log.c \
    dragon_ai.c \
    loki_config.c \
    main.c \
    memory.c \
    plugin_manager.c \
    pwm.c \
    retry.c \
    sdcard_driver.c \
    spi.c \
    system.c \
    tft_driver.c \
    tool_runner.c \
    uart.c \
    web_ui.c \
    -o "${BUILD_DIR}/loki_app" \
    -lm -lpthread

chmod +x "${BUILD_DIR}/loki_app"

echo "[OK] Built ${BUILD_DIR}/loki_app"
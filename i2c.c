/**
 * I2C Hardware Abstraction Layer Implementation
 * Raspberry Pi
 */

#include "i2c.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

/* ===== I2C DEVICE CONTEXT ===== */
typedef struct {
    i2c_bus_t bus;
    int device_handle;
    i2c_config_t config;
    uint8_t initialized;
} i2c_context_t;

/* I2C bus context */
static i2c_context_t i2c_context_0 = {
    .bus = I2C_BUS_0,
    .initialized = 0,
    .device_handle = -1,
};

/* ===== LOCAL HELPERS ===== */

static int i2c_open_first_available(void)
{
    const char *candidates[] = {"/dev/i2c-1", "/dev/i2c-0"};
    size_t i;

    for (i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); i++) {
        int fd = open(candidates[i], O_RDWR);
        if (fd >= 0) {
            LOG_INFO("I2C using device %s", candidates[i]);
            return fd;
        }
        LOG_WARN("Unable to open %s: %s", candidates[i], strerror(errno));
    }

    return -1;
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t i2c_init(i2c_bus_t bus, const i2c_config_t *config)
{
    if (config == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (bus != I2C_BUS_0) {
        return HAL_INVALID_PARAM;
    }

    i2c_context_t *ctx = &i2c_context_0;

    if (ctx->initialized) {
        return HAL_OK;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(i2c_config_t));

    ctx->device_handle = i2c_open_first_available();
    if (ctx->device_handle < 0) {
        LOG_ERROR("I2C init failed: no usable /dev/i2c-* device found");
        return HAL_ERROR;
    }

    ctx->initialized = 1;
    return HAL_OK;
}

hal_status_t i2c_write(i2c_bus_t bus, uint8_t device_addr, const uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0 || device_addr == 0) {
        return HAL_INVALID_PARAM;
    }

    if (bus != I2C_BUS_0) {
        return HAL_INVALID_PARAM;
    }

    i2c_context_t *ctx = &i2c_context_0;

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    if (ioctl(ctx->device_handle, I2C_SLAVE, device_addr) < 0) {
        LOG_ERROR("I2C write failed to set slave 0x%02X: %s", device_addr, strerror(errno));
        return HAL_ERROR;
    }

    ssize_t written = write(ctx->device_handle, data, length);
    if (written < 0 || (uint32_t)written != length) {
        LOG_ERROR("I2C write failed to 0x%02X: %s", device_addr, strerror(errno));
        return HAL_ERROR;
    }

    return HAL_OK;
}

hal_status_t i2c_read(i2c_bus_t bus, uint8_t device_addr, uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0 || device_addr == 0) {
        return HAL_INVALID_PARAM;
    }

    if (bus != I2C_BUS_0) {
        return HAL_INVALID_PARAM;
    }

    i2c_context_t *ctx = &i2c_context_0;

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    if (ioctl(ctx->device_handle, I2C_SLAVE, device_addr) < 0) {
        LOG_ERROR("I2C read failed to set slave 0x%02X: %s", device_addr, strerror(errno));
        return HAL_ERROR;
    }

    ssize_t bytes_read = read(ctx->device_handle, data, length);
    if (bytes_read < 0 || (uint32_t)bytes_read != length) {
        LOG_ERROR("I2C read failed from 0x%02X: %s", device_addr, strerror(errno));
        return HAL_ERROR;
    }

    return HAL_OK;
}

hal_status_t i2c_write_read(i2c_bus_t bus, uint8_t device_addr,
                           const uint8_t *tx_data, uint32_t tx_length,
                           uint8_t *rx_data, uint32_t rx_length)
{
    if (device_addr == 0 || tx_data == NULL || rx_data == NULL || tx_length == 0 || rx_length == 0) {
        return HAL_INVALID_PARAM;
    }
    if (tx_length > 0xFFFFu || rx_length > 0xFFFFu) {
        return HAL_INVALID_PARAM;
    }

    if (bus != I2C_BUS_0) {
        return HAL_INVALID_PARAM;
    }

    i2c_context_t *ctx = &i2c_context_0;

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data xfer;

    msgs[0].addr = device_addr;
    msgs[0].flags = 0;
    msgs[0].len = (__u16)tx_length;
    msgs[0].buf = (uint8_t *)tx_data;

    msgs[1].addr = device_addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = (__u16)rx_length;
    msgs[1].buf = rx_data;

    xfer.msgs = msgs;
    xfer.nmsgs = 2;

    if (ioctl(ctx->device_handle, I2C_RDWR, &xfer) < 0) {
        LOG_ERROR("I2C write/read failed for 0x%02X: %s", device_addr, strerror(errno));
        return HAL_ERROR;
    }

    return HAL_OK;
}

hal_status_t i2c_deinit(i2c_bus_t bus)
{
    if (bus != I2C_BUS_0) {
        return HAL_INVALID_PARAM;
    }

    i2c_context_t *ctx = &i2c_context_0;

    if (!ctx->initialized) {
        return HAL_OK;
    }

    if (ctx->device_handle >= 0) {
        close(ctx->device_handle);
        ctx->device_handle = -1;
    }

    ctx->initialized = 0;
    return HAL_OK;
}

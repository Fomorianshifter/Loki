/**
 * I2C Hardware Abstraction Layer - Actual Implementation
 * Uses Linux /dev/i2c-* interface
 */

#include "i2c.h"
#include "../../utils/log.h"
#include "../../config/platform.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <errno.h>

/* ===== I2C DEVICE CONTEXT ===== */
typedef struct {
    i2c_bus_t bus;
    int device_handle;
    i2c_config_t config;
    uint8_t initialized;
} i2c_context_t;

static i2c_context_t i2c_contexts[I2C_BUS_COUNT] = {{0}};

/* ===== HELPER FUNCTIONS ===== */
static hal_status_t i2c_open_device(i2c_bus_t bus)
{
    char dev_path[32];
    snprintf(dev_path, sizeof(dev_path), "/dev/i2c-%d", bus);
    
    int fd = open(dev_path, O_RDWR);
    if (fd < 0) {
        LOG_ERROR("Failed to open %s: %s", dev_path, strerror(errno));
        return HAL_ERROR;
    }
    
    LOG_DEBUG("Opened I2C device: %s (fd=%d)", dev_path, fd);
    return HAL_OK;
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t i2c_init(i2c_bus_t bus, const i2c_config_t *config)
{
    if (config == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (bus >= I2C_BUS_COUNT) {
        return HAL_INVALID_PARAM;
    }

    i2c_context_t *ctx = &i2c_contexts[bus];

    if (ctx->initialized) {
        return HAL_OK;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(i2c_config_t));
    ctx->bus = bus;

    /* Open I2C device */
    char dev_path[32];
    snprintf(dev_path, sizeof(dev_path), "/dev/i2c-%d", bus);
    
    ctx->device_handle = open(dev_path, O_RDWR);
    if (ctx->device_handle < 0) {
        LOG_ERROR("Failed to open I2C bus %d: %s", bus, strerror(errno));
        return HAL_ERROR;
    }

    /* Set I2C bus speed */
    uint32_t speed = config->frequency;
    if (ioctl(ctx->device_handle, I2C_TIMEOUT, 1) < 0) {
        LOG_WARN("Failed to set I2C timeout");
    }

    ctx->initialized = 1;
    LOG_INFO("I2C bus %d initialized (freq=%u Hz)", bus, speed);
    return HAL_OK;
}

hal_status_t i2c_write(i2c_bus_t bus, uint8_t device_addr, const uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0 || device_addr == 0) {
        return HAL_INVALID_PARAM;
    }

    if (bus >= I2C_BUS_COUNT) {
        return HAL_INVALID_PARAM;
    }

    i2c_context_t *ctx = &i2c_contexts[bus];

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    /* Set slave address */
    if (ioctl(ctx->device_handle, I2C_SLAVE, device_addr) < 0) {
        LOG_ERROR("Failed to set I2C slave address 0x%02X: %s", device_addr, strerror(errno));
        return HAL_ERROR;
    }

    /* Write data */
    ssize_t result = write(ctx->device_handle, data, length);
    if (result != (ssize_t)length) {
        LOG_ERROR("I2C write failed (addr=0x%02X, wrote %ld/%u bytes)", device_addr, result, length);
        return HAL_ERROR;
    }

    LOG_DEBUG("I2C write: addr=0x%02X, length=%u", device_addr, length);
    return HAL_OK;
}

hal_status_t i2c_read(i2c_bus_t bus, uint8_t device_addr, uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0 || device_addr == 0) {
        return HAL_INVALID_PARAM;
    }

    if (bus >= I2C_BUS_COUNT) {
        return HAL_INVALID_PARAM;
    }

    i2c_context_t *ctx = &i2c_contexts[bus];

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    /* Set slave address */
    if (ioctl(ctx->device_handle, I2C_SLAVE, device_addr) < 0) {
        LOG_ERROR("Failed to set I2C slave address 0x%02X: %s", device_addr, strerror(errno));
        return HAL_ERROR;
    }

    /* Read data */
    ssize_t result = read(ctx->device_handle, data, length);
    if (result != (ssize_t)length) {
        LOG_ERROR("I2C read failed (addr=0x%02X, read %ld/%u bytes)", device_addr, result, length);
        return HAL_ERROR;
    }

    LOG_DEBUG("I2C read: addr=0x%02X, length=%u", device_addr, length);
    return HAL_OK;
}

hal_status_t i2c_write_read(i2c_bus_t bus, uint8_t device_addr,
                            const uint8_t *tx_data, uint32_t tx_length,
                            uint8_t *rx_data, uint32_t rx_length)
{
    if (device_addr == 0 || tx_data == NULL || rx_data == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (bus >= I2C_BUS_COUNT) {
        return HAL_INVALID_PARAM;
    }

    i2c_context_t *ctx = &i2c_contexts[bus];

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    /* Perform write operation */
    hal_status_t status = i2c_write(bus, device_addr, tx_data, tx_length);
    if (status != HAL_OK) {
        return status;
    }

    /* Small delay for device processing */
    usleep(1000);

    /* Perform read operation */
    status = i2c_read(bus, device_addr, rx_data, rx_length);
    if (status != HAL_OK) {
        return status;
    }

    return HAL_OK;
}

hal_status_t i2c_deinit(i2c_bus_t bus)
{
    if (bus >= I2C_BUS_COUNT) {
        return HAL_INVALID_PARAM;
    }

    i2c_context_t *ctx = &i2c_contexts[bus];

    if (!ctx->initialized) {
        return HAL_OK;
    }

    if (ctx->device_handle >= 0) {
        close(ctx->device_handle);
        ctx->device_handle = -1;
    }

    ctx->initialized = 0;
    LOG_INFO("I2C bus %d deinitialized", bus);
    return HAL_OK;
}

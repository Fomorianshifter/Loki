/**
 * I2C Hardware Abstraction Layer Implementation
 * Orange Pi Zero 2W
 */

#include "i2c.h"
#include <stdio.h>
#include <string.h>

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
};

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

    /* Open I2C device at /dev/i2c-0 */
    /* Configure frequency and address mode */

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

    /* Set device address */
    /* Send START condition */
    /* Transmit address byte with write bit (LSB=0) */
    /* Send data bytes */
    /* Send STOP condition */

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

    /* Set device address */
    /* Send START condition */
    /* Transmit address byte with read bit (LSB=1) */
    /* Receive data bytes with ACK/NACK handshake */
    /* Send STOP condition */

    return HAL_OK;
}

hal_status_t i2c_write_read(i2c_bus_t bus, uint8_t device_addr,
                           const uint8_t *tx_data, uint32_t tx_length,
                           uint8_t *rx_data, uint32_t rx_length)
{
    if (device_addr == 0 || tx_data == NULL || rx_data == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (bus != I2C_BUS_0) {
        return HAL_INVALID_PARAM;
    }

    i2c_context_t *ctx = &i2c_context_0;

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    /* Perform write operation */
    hal_status_t status = i2c_write(bus, device_addr, tx_data, tx_length);
    if (status != HAL_OK) {
        return status;
    }

    /* Perform read operation (REPEATED START) */
    status = i2c_read(bus, device_addr, rx_data, rx_length);
    if (status != HAL_OK) {
        return status;
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

    /* Close I2C device */
    /* if (ctx->device_handle >= 0) {
        close(ctx->device_handle);
    } */

    ctx->initialized = 0;
    return HAL_OK;
}

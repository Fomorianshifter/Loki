/**
 * SPI Hardware Abstraction Layer - Actual Implementation
 * Uses Linux /dev/spidev* interface
 */

#include "spi.h"
#include "../../utils/log.h"
#include "../../config/platform.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <errno.h>

/* ===== SPI DEVICE CONTEXT ===== */
typedef struct {
    spi_bus_t bus;
    int device_handle;
    spi_config_t config;
    uint8_t initialized;
} spi_context_t;

static spi_context_t spi_contexts[SPI_BUS_COUNT] = {{0}};

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t spi_init(spi_bus_t bus, const spi_config_t *config)
{
    if (config == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (bus >= SPI_BUS_COUNT) {
        return HAL_INVALID_PARAM;
    }

    spi_context_t *ctx = &spi_contexts[bus];

    if (ctx->initialized) {
        return HAL_OK;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(spi_config_t));
    ctx->bus = bus;

    /* Open SPI device */
    char dev_path[32];
    snprintf(dev_path, sizeof(dev_path), "/dev/spidev0.%d", bus);
    
    ctx->device_handle = open(dev_path, O_RDWR);
    if (ctx->device_handle < 0) {
        LOG_ERROR("Failed to open SPI bus %d: %s", bus, strerror(errno));
        return HAL_ERROR;
    }

    /* Configure SPI mode */
    uint8_t mode = config->mode;
    if (ioctl(ctx->device_handle, SPI_IOC_WR_MODE, &mode) < 0) {
        LOG_ERROR("Failed to set SPI mode: %s", strerror(errno));
        close(ctx->device_handle);
        return HAL_ERROR;
    }

    /* Configure bits per word */
    uint8_t bits = config->bits_per_word;
    if (ioctl(ctx->device_handle, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        LOG_ERROR("Failed to set SPI bits per word: %s", strerror(errno));
        close(ctx->device_handle);
        return HAL_ERROR;
    }

    /* Configure frequency */
    uint32_t speed = config->frequency;
    if (ioctl(ctx->device_handle, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        LOG_ERROR("Failed to set SPI speed: %s", strerror(errno));
        close(ctx->device_handle);
        return HAL_ERROR;
    }

    ctx->initialized = 1;
    LOG_INFO("SPI bus %d initialized (freq=%u Hz, mode=%d)", bus, speed, mode);
    return HAL_OK;
}

hal_status_t spi_write(spi_bus_t bus, uint32_t cs_pin, const uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0) {
        return HAL_INVALID_PARAM;
    }

    if (bus >= SPI_BUS_COUNT) {
        return HAL_INVALID_PARAM;
    }

    spi_context_t *ctx = &spi_contexts[bus];

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    ssize_t result = write(ctx->device_handle, data, length);
    if (result != (ssize_t)length) {
        LOG_ERROR("SPI write failed (bus=%d, wrote %ld/%u bytes)", bus, result, length);
        return HAL_ERROR;
    }

    LOG_DEBUG("SPI write: bus=%d, length=%u", bus, length);
    return HAL_OK;
}

hal_status_t spi_read(spi_bus_t bus, uint32_t cs_pin, uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0) {
        return HAL_INVALID_PARAM;
    }

    if (bus >= SPI_BUS_COUNT) {
        return HAL_INVALID_PARAM;
    }

    spi_context_t *ctx = &spi_contexts[bus];

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    ssize_t result = read(ctx->device_handle, data, length);
    if (result != (ssize_t)length) {
        LOG_ERROR("SPI read failed (bus=%d, read %ld/%u bytes)", bus, result, length);
        return HAL_ERROR;
    }

    LOG_DEBUG("SPI read: bus=%d, length=%u", bus, length);
    return HAL_OK;
}

hal_status_t spi_transfer(spi_bus_t bus, uint32_t cs_pin, 
                          const uint8_t *tx_data, uint32_t tx_length,
                          uint8_t *rx_data, uint32_t rx_length)
{
    if ((tx_data == NULL || tx_length == 0) || (rx_data == NULL || rx_length == 0)) {
        return HAL_INVALID_PARAM;
    }

    if (bus >= SPI_BUS_COUNT) {
        return HAL_INVALID_PARAM;
    }

    spi_context_t *ctx = &spi_contexts[bus];

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    /* Use full-duplex SPI transfer */
    struct spi_ioc_transfer xfer[1];
    memset(xfer, 0, sizeof(xfer));
    
    xfer[0].tx_buf = (unsigned long)tx_data;
    xfer[0].rx_buf = (unsigned long)rx_data;
    xfer[0].len = (tx_length > rx_length) ? tx_length : rx_length;
    xfer[0].speed_hz = ctx->config.frequency;
    xfer[0].bits_per_word = ctx->config.bits_per_word;

    int result = ioctl(ctx->device_handle, SPI_IOC_MESSAGE(1), xfer);
    if (result < 0) {
        LOG_ERROR("SPI transfer failed (bus=%d): %s", bus, strerror(errno));
        return HAL_ERROR;
    }

    LOG_DEBUG("SPI transfer: bus=%d, tx=%u bytes, rx=%u bytes", bus, tx_length, rx_length);
    return HAL_OK;
}

hal_status_t spi_deinit(spi_bus_t bus)
{
    if (bus >= SPI_BUS_COUNT) {
        return HAL_INVALID_PARAM;
    }

    spi_context_t *ctx = &spi_contexts[bus];

    if (!ctx->initialized) {
        return HAL_OK;
    }

    if (ctx->device_handle >= 0) {
        close(ctx->device_handle);
        ctx->device_handle = -1;
    }

    ctx->initialized = 0;
    LOG_INFO("SPI bus %d deinitialized", bus);
    return HAL_OK;
}

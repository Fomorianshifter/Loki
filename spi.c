/**
 * SPI Hardware Abstraction Layer Implementation
 * Orange Pi Zero 2W
 */

#include "spi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <linux/spi/spidev.h>

/* ===== SPI DEVICE CONTEXT ===== */
typedef struct {
    spi_bus_t bus;
    int device_handle;
    spi_config_t config;
    uint8_t initialized;
} spi_context_t;

/* SPI bus contexts */
static spi_context_t spi_contexts[SPI_BUS_COUNT] = {
    {.bus = SPI_BUS_0, .initialized = 0, .device_handle = -1},
    {.bus = SPI_BUS_1, .initialized = 0, .device_handle = -1},
    {.bus = SPI_BUS_2, .initialized = 0, .device_handle = -1},
};

/* ===== LOCAL HELPER FUNCTIONS ===== */

/**
 * Validate SPI bus number
 */
static hal_status_t spi_validate_bus(spi_bus_t bus)
{
    if (bus >= SPI_BUS_COUNT) {
        return HAL_INVALID_PARAM;
    }
    return HAL_OK;
}

/**
 * Get SPI device path from bus number
 */
static const char* spi_get_device_path(spi_bus_t bus)
{
    /* Orange Pi Zero 2W SPI device mapping */
    switch (bus) {
        case SPI_BUS_0: return "/dev/spidev0.0";
        case SPI_BUS_1: return "/dev/spidev1.0";
        case SPI_BUS_2: return "/dev/spidev1.1";
        default: return NULL;
    }
}

/**
 * Open SPI device file
 */
static hal_status_t spi_open_device(spi_bus_t bus, spi_context_t *ctx)
{
    const char *device_path = spi_get_device_path(bus);
    if (device_path == NULL) {
        return HAL_INVALID_PARAM;
    }

    int fd = open(device_path, O_RDWR);
    if (fd < 0) {
        return HAL_ERROR;
    }

    ctx->device_handle = fd;
    return HAL_OK;
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t spi_init(spi_bus_t bus, const spi_config_t *config)
{
    if (config == NULL) {
        return HAL_INVALID_PARAM;
    }

    hal_status_t status = spi_validate_bus(bus);
    if (status != HAL_OK) {
        return status;
    }

    spi_context_t *ctx = &spi_contexts[bus];
    if (ctx->initialized) {
        return HAL_OK;  /* Already initialized */
    }

    /* Open SPI device */
    status = spi_open_device(bus, ctx);
    if (status != HAL_OK) {
        return status;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(spi_config_t));

    /* Set SPI mode */
    uint8_t mode = config->mode;
    if (ioctl(ctx->device_handle, SPI_IOC_WR_MODE, &mode) < 0) {
        close(ctx->device_handle);
        ctx->device_handle = -1;
        return HAL_ERROR;
    }

    /* Set bits per word */
    uint8_t bits = config->bits_per_word;
    if (ioctl(ctx->device_handle, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        close(ctx->device_handle);
        ctx->device_handle = -1;
        return HAL_ERROR;
    }

    /* Set SPI clock speed */
    uint32_t speed = config->frequency;
    if (ioctl(ctx->device_handle, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        close(ctx->device_handle);
        ctx->device_handle = -1;
        return HAL_ERROR;
    }

    /* Set LSB/MSB first */
    uint8_t lsb_first = (config->bit_order == SPI_LSB_FIRST) ? 1 : 0;
    if (ioctl(ctx->device_handle, SPI_IOC_WR_LSB_FIRST, &lsb_first) < 0) {
        close(ctx->device_handle);
        ctx->device_handle = -1;
        return HAL_ERROR;
    }

    ctx->initialized = 1;
    return HAL_OK;
}

hal_status_t spi_write(spi_bus_t bus, uint32_t cs_pin, const uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0) {
        return HAL_INVALID_PARAM;
    }

    hal_status_t status = spi_validate_bus(bus);
    if (status != HAL_OK) {
        return status;
    }

    spi_context_t *ctx = &spi_contexts[bus];
    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    /* Perform SPI write (transmit only) */
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)data,
        .len = length,
        .speed_hz = ctx->config.frequency,
        .bits_per_word = ctx->config.bits_per_word,
        .delay_usecs = 0,
        .cs_change = 0,
    };

    if (ioctl(ctx->device_handle, SPI_IOC_MESSAGE(1), &tr) < 0) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

hal_status_t spi_read(spi_bus_t bus, uint32_t cs_pin, uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0) {
        return HAL_INVALID_PARAM;
    }

    hal_status_t status = spi_validate_bus(bus);
    if (status != HAL_OK) {
        return status;
    }

    spi_context_t *ctx = &spi_contexts[bus];
    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    /* Perform SPI read (receive only) */
    struct spi_ioc_transfer tr = {
        .rx_buf = (unsigned long)data,
        .len = length,
        .speed_hz = ctx->config.frequency,
        .bits_per_word = ctx->config.bits_per_word,
        .delay_usecs = 0,
        .cs_change = 0,
    };

    if (ioctl(ctx->device_handle, SPI_IOC_MESSAGE(1), &tr) < 0) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

hal_status_t spi_transfer(spi_bus_t bus, uint32_t cs_pin,
                         const uint8_t *tx_data, uint32_t tx_length,
                         uint8_t *rx_data, uint32_t rx_length)
{
    if ((tx_data == NULL && rx_data == NULL) || (tx_length == 0 && rx_length == 0)) {
        return HAL_INVALID_PARAM;
    }

    hal_status_t status = spi_validate_bus(bus);
    if (status != HAL_OK) {
        return status;
    }

    spi_context_t *ctx = &spi_contexts[bus];
    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    /* For read operations after write, we need to handle both */
    if (tx_data != NULL && tx_length > 0) {
        struct spi_ioc_transfer tr = {
            .tx_buf = (unsigned long)tx_data,
            .len = tx_length,
            .speed_hz = ctx->config.frequency,
            .bits_per_word = ctx->config.bits_per_word,
            .delay_usecs = 0,
            .cs_change = (rx_data != NULL && rx_length > 0) ? 1 : 0,
        };

        if (ioctl(ctx->device_handle, SPI_IOC_MESSAGE(1), &tr) < 0) {
            return HAL_ERROR;
        }
    }

    if (rx_data != NULL && rx_length > 0) {
        struct spi_ioc_transfer tr = {
            .rx_buf = (unsigned long)rx_data,
            .len = rx_length,
            .speed_hz = ctx->config.frequency,
            .bits_per_word = ctx->config.bits_per_word,
            .delay_usecs = 0,
            .cs_change = 0,
        };

        if (ioctl(ctx->device_handle, SPI_IOC_MESSAGE(1), &tr) < 0) {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

hal_status_t spi_deinit(spi_bus_t bus)
{
    hal_status_t status = spi_validate_bus(bus);
    if (status != HAL_OK) {
        return status;
    }

    spi_context_t *ctx = &spi_contexts[bus];
    
    if (!ctx->initialized) {
        return HAL_OK;
    }

    /* Close SPI device */
    if (ctx->device_handle >= 0) {
        close(ctx->device_handle);
        ctx->device_handle = -1;
    }

    ctx->initialized = 0;
    return HAL_OK;
}

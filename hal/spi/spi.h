#ifndef SPI_H
#define SPI_H

/**
 * SPI Hardware Abstraction Layer for Orange Pi Zero 2W
 * Supports SPI0 (TFT), SPI1 (SD Card), and SPI2 (Flash)
 */

#include "../../includes/types.h"

/* ===== SPI BUS DEFINITIONS ===== */
typedef enum {
    SPI_BUS_0 = 0,  /* TFT Display */
    SPI_BUS_1 = 1,  /* SD Card */
    SPI_BUS_2 = 2,  /* Loki Credits Flash */
    SPI_BUS_COUNT = 3,
} spi_bus_t;

/* ===== PUBLIC API ===== */

/**
 * Initialize SPI bus
 * @param[in] bus SPI bus number (0, 1, or 2)
 * @param[in] config SPI configuration
 * @return HAL_OK on success
 */
hal_status_t spi_init(spi_bus_t bus, const spi_config_t *config);

/**
 * Write data to SPI bus
 * @param[in] bus SPI bus number
 * @param[in] cs_pin Chip select pin
 * @param[in] data Pointer to data buffer
 * @param[in] length Number of bytes to write
 * @return HAL_OK on success
 */
hal_status_t spi_write(spi_bus_t bus, uint32_t cs_pin, const uint8_t *data, uint32_t length);

/**
 * Read data from SPI bus
 * @param[in] bus SPI bus number
 * @param[in] cs_pin Chip select pin
 * @param[out] data Pointer to receive buffer
 * @param[in] length Number of bytes to read
 * @return HAL_OK on success
 */
hal_status_t spi_read(spi_bus_t bus, uint32_t cs_pin, uint8_t *data, uint32_t length);

/**
 * SPI transfer (write then read)
 * @param[in] bus SPI bus number
 * @param[in] cs_pin Chip select pin
 * @param[in] tx_data Transmit buffer
 * @param[in] tx_length Transmit length
 * @param[out] rx_data Receive buffer
 * @param[in] rx_length Receive length
 * @return HAL_OK on success
 */
hal_status_t spi_transfer(spi_bus_t bus, uint32_t cs_pin, 
                         const uint8_t *tx_data, uint32_t tx_length,
                         uint8_t *rx_data, uint32_t rx_length);

/**
 * Deinitialize SPI bus
 * @param[in] bus SPI bus number
 * @return HAL_OK on success
 */
hal_status_t spi_deinit(spi_bus_t bus);

#endif /* SPI_H */

#ifndef I2C_H
#define I2C_H

/**
 * I2C Hardware Abstraction Layer for Orange Pi Zero 2W
 * Supports I2C0 for EEPROM communication
 */

#include "../../includes/types.h"

/* ===== I2C BUS DEFINITIONS ===== */
typedef enum {
    I2C_BUS_0 = 0,  /* EEPROM, and optional sensors */
    I2C_BUS_COUNT = 1,
} i2c_bus_t;

/* ===== PUBLIC API ===== */

/**
 * Initialize I2C bus
 * @param[in] bus I2C bus number
 * @param[in] config I2C configuration
 * @return HAL_OK on success
 */
hal_status_t i2c_init(i2c_bus_t bus, const i2c_config_t *config);

/**
 * Write data to I2C device
 * @param[in] bus I2C bus number
 * @param[in] device_addr 7-bit I2C device address
 * @param[in] data Pointer to data buffer
 * @param[in] length Number of bytes to write
 * @return HAL_OK on success
 */
hal_status_t i2c_write(i2c_bus_t bus, uint8_t device_addr, const uint8_t *data, uint32_t length);

/**
 * Read data from I2C device
 * @param[in] bus I2C bus number
 * @param[in] device_addr 7-bit I2C device address
 * @param[out] data Pointer to receive buffer
 * @param[in] length Number of bytes to read
 * @return HAL_OK on success
 */
hal_status_t i2c_read(i2c_bus_t bus, uint8_t device_addr, uint8_t *data, uint32_t length);

/**
 * Write then read from I2C device (common pattern)
 * @param[in] bus I2C bus number
 * @param[in] device_addr 7-bit I2C device address
 * @param[in] tx_data Transmit buffer
 * @param[in] tx_length Transmit length
 * @param[out] rx_data Receive buffer
 * @param[in] rx_length Receive length
 * @return HAL_OK on success
 */
hal_status_t i2c_write_read(i2c_bus_t bus, uint8_t device_addr,
                           const uint8_t *tx_data, uint32_t tx_length,
                           uint8_t *rx_data, uint32_t rx_length);

/**
 * Deinitialize I2C bus
 * @param[in] bus I2C bus number
 * @return HAL_OK on success
 */
hal_status_t i2c_deinit(i2c_bus_t bus);

#endif /* I2C_H */

#ifndef EEPROM_DRIVER_H
#define EEPROM_DRIVER_H

/**
 * EEPROM Driver for FT24C02A (256 bytes)
 * Communicates via I2C0
 */

#include "types.h"
#include "config.h"

/* ===== EEPROM INTERFACE ===== */

/**
 * Initialize EEPROM
 * @return HAL_OK on success
 */
hal_status_t eeprom_init(void);

/**
 * Check whether EEPROM is initialized and ready.
 * @return 1 when ready, 0 otherwise
 */
uint8_t eeprom_is_ready(void);

/**
 * Read from EEPROM
 * @param[in] address Memory address (0-255)
 * @param[out] buffer Receive buffer
 * @param[in] length Number of bytes to read
 * @return HAL_OK on success
 */
hal_status_t eeprom_read(uint8_t address, uint8_t *buffer, uint16_t length);

/**
 * Write to EEPROM
 * @param[in] address Memory address (0-255)
 * @param[in] buffer Data buffer
 * @param[in] length Number of bytes to write
 * @return HAL_OK on success
 */
hal_status_t eeprom_write(uint8_t address, const uint8_t *buffer, uint16_t length);

/**
 * Deinitialize EEPROM
 * @return HAL_OK on success
 */
hal_status_t eeprom_deinit(void);

#endif /* EEPROM_DRIVER_H */

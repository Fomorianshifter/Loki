#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

/**
 * SPI Flash Driver for Loki Credits Flash (W25Q40)
 * Communicates via SPI2
 * 4 Megabit (512 KiB) capacity
 */

#include "../../includes/types.h"
#include "../../config/board_config.h"

/* ===== LOKI CREDITS FLASH INTERFACE ===== */

/**
 * Initialize flash memory
 * @return HAL_OK on success
 */
hal_status_t flash_init(void);

/**
 * Read from flash memory
 * @param[in] address Memory address
 * @param[out] buffer Receive buffer
 * @param[in] length Number of bytes to read
 * @return HAL_OK on success
 */
hal_status_t flash_read(uint32_t address, uint8_t *buffer, uint32_t length);

/**
 * Write to flash memory (page write)
 * @param[in] address Memory address
 * @param[in] buffer Data buffer
 * @param[in] length Number of bytes to write (max 256)
 * @return HAL_OK on success
 */
hal_status_t flash_write(uint32_t address, const uint8_t *buffer, uint32_t length);

/**
 * Erase flash sector (4 KiB)
 * @param[in] address Sector address (must be sector-aligned)
 * @return HAL_OK on success
 */
hal_status_t flash_erase_sector(uint32_t address);

/**
 * Erase entire flash
 * @return HAL_OK on success
 */
hal_status_t flash_erase_all(void);

/**
 * Get flash JEDEC ID
 * @param[out] jedec_id Pointer to store JEDEC ID (3 bytes)
 * @return HAL_OK on success
 */
hal_status_t flash_get_jedec_id(uint8_t *jedec_id);

/**
 * Deinitialize flash memory
 * @return HAL_OK on success
 */
hal_status_t flash_deinit(void);

#endif /* FLASH_DRIVER_H */

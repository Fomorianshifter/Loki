#ifndef SDCARD_DRIVER_H
#define SDCARD_DRIVER_H

/**
 * SD Card Driver for push-pull 6-pin SD module
 * Communicates via SPI1
 */

#include "../../includes/types.h"
#include "../../config/board_config.h"

/* ===== SD CARD INTERFACE ===== */

/**
 * Initialize SD card
 * @return HAL_OK on success
 */
hal_status_t sdcard_init(void);

/**
 * Read SD card sector(s)
 * @param[in] sector Sector number to read
 * @param[in] count Number of sectors (usually 1)
 * @param[out] buffer Pointer to receive buffer (min 512 bytes per sector)
 * @return HAL_OK on success
 */
hal_status_t sdcard_read_sector(uint32_t sector, uint16_t count, uint8_t *buffer);

/**
 * Write SD card sector(s)
 * @param[in] sector Sector number to write
 * @param[in] count Number of sectors
 * @param[in] buffer Pointer to data buffer (512 bytes per sector)
 * @return HAL_OK on success
 */
hal_status_t sdcard_write_sector(uint32_t sector, uint16_t count, const uint8_t *buffer);

/**
 * Get SD card capacity
 * @param[out] capacity Pointer to store capacity in bytes
 * @return HAL_OK on success
 */
hal_status_t sdcard_get_capacity(uint32_t *capacity);

/**
 * Deinitialize SD card
 * @return HAL_OK on success
 */
hal_status_t sdcard_deinit(void);

#endif /* SDCARD_DRIVER_H */

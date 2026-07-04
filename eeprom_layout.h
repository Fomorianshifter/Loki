#ifndef EEPROM_LAYOUT_H
#define EEPROM_LAYOUT_H

/**
 * @file eeprom_layout.h
 * @brief EEPROM bootstrap/failsafe memory map for Loki.
 *
 * The FT24C02A has only 256 bytes of EEPROM.  It must NOT be used as a
 * full configuration store.  Only small, critical bootstrap metadata
 * belongs here — everything else lives in loki_config.ini on the SD card.
 *
 * Memory map (16 bytes total, leaving 240 bytes reserved for future use):
 *
 *  Offset  Size  Field                  Description
 *  ------  ----  ---------------------  -----------------------------------
 *    0      2    EEPROM_MAGIC           0x4C4B ('L','K') — sanity marker
 *    2      1    EEPROM_SCHEMA_VER      Layout version (this file = 1)
 *    3      1    EEPROM_CONFIG_VER      SD config schema version last seen
 *    4      2    EEPROM_BOOT_COUNT      Cumulative boot counter (saturates at 65535)
 *    6      1    EEPROM_SAFE_MODE_FLAG  1 = boot into safe mode next time
 *    7      1    EEPROM_SD_CONFIG_EN    1 = SD config enabled (0 = use defaults)
 *    8      1    EEPROM_BRIGHTNESS      Last-known-good brightness (0–100)
 *    9      1    EEPROM_STARTUP_MODE    0=normal, 1=safe, 2=demo
 *   10      4    EEPROM_RESERVED        Reserved — must be written as 0x00
 *   14      2    EEPROM_CRC16           CRC-16/CCITT over bytes 0–13
 *
 * Total bootstrap block: 16 bytes (offsets 16–255 are free for future use).
 *
 * IMPORTANT: After every write to the bootstrap block, update the CRC16
 * field using eeprom_bootstrap_write().  Read-back validation uses
 * eeprom_bootstrap_read() which checks magic + CRC before trusting data.
 */

#include "types.h"

/* ===== MAGIC NUMBER ===== */
#define EEPROM_MAGIC_BYTE0      0x4C    /* 'L' */
#define EEPROM_MAGIC_BYTE1      0x4B    /* 'K' */
#define EEPROM_MAGIC_VALUE      0x4C4B

/* ===== SCHEMA VERSION ===== */
#define EEPROM_SCHEMA_VERSION   1       /* Increment when layout changes */

/* ===== BYTE OFFSETS ===== */
#define EEPROM_OFF_MAGIC        0       /* 2 bytes */
#define EEPROM_OFF_SCHEMA_VER   2       /* 1 byte  */
#define EEPROM_OFF_CONFIG_VER   3       /* 1 byte  */
#define EEPROM_OFF_BOOT_COUNT   4       /* 2 bytes (little-endian) */
#define EEPROM_OFF_SAFE_MODE    6       /* 1 byte  */
#define EEPROM_OFF_SD_CFG_EN    7       /* 1 byte  */
#define EEPROM_OFF_BRIGHTNESS   8       /* 1 byte  */
#define EEPROM_OFF_STARTUP_MODE 9       /* 1 byte  */
#define EEPROM_OFF_RESERVED     10      /* 4 bytes */
#define EEPROM_OFF_CRC16        14      /* 2 bytes (little-endian) */

#define EEPROM_BOOTSTRAP_SIZE   16      /* Total bytes in bootstrap block */
#define EEPROM_CRC_DATA_LEN     14      /* Bytes covered by CRC (0–13) */

/* ===== STARTUP MODE CODES ===== */
#define EEPROM_MODE_NORMAL      0
#define EEPROM_MODE_SAFE        1
#define EEPROM_MODE_DEMO        2

/* ===== BOOTSTRAP STRUCT ===== */
/**
 * In-memory representation of the EEPROM bootstrap block.
 * Use eeprom_bootstrap_read() / eeprom_bootstrap_write() to access EEPROM.
 */
typedef struct {
    uint16_t magic;             /**< Must equal EEPROM_MAGIC_VALUE         */
    uint8_t  schema_ver;        /**< EEPROM layout version                 */
    uint8_t  config_ver;        /**< Last SD config schema version seen    */
    uint16_t boot_count;        /**< Cumulative boot counter               */
    uint8_t  safe_mode_flag;    /**< 1 = force safe mode on next boot      */
    uint8_t  sd_config_enabled; /**< 1 = load config from SD card          */
    uint8_t  brightness;        /**< Last-known-good display brightness    */
    uint8_t  startup_mode;      /**< EEPROM_MODE_* constant                */
    uint8_t  reserved[4];       /**< Zero-fill; reserved for future use    */
    uint16_t crc16;             /**< CRC-16/CCITT over first 14 bytes      */
} eeprom_bootstrap_t;

/* ===== API ===== */

/**
 * Read and validate bootstrap data from EEPROM.
 *
 * Validates magic number and CRC.  On failure (first boot or corruption)
 * the struct is populated with safe defaults and HAL_ERROR is returned so
 * the caller can re-initialise the EEPROM block.
 *
 * @param[out] bs  Destination struct
 * @return HAL_OK on success, HAL_ERROR if magic/CRC mismatch
 */
hal_status_t eeprom_bootstrap_read(eeprom_bootstrap_t *bs);

/**
 * Write bootstrap data to EEPROM (recomputes CRC automatically).
 *
 * @param[in] bs  Source struct (crc16 field is ignored and recomputed)
 * @return HAL_OK on success
 */
hal_status_t eeprom_bootstrap_write(const eeprom_bootstrap_t *bs);

/**
 * Populate *bs with safe factory defaults (does NOT write to EEPROM).
 *
 * @param[out] bs  Destination struct
 */
void eeprom_bootstrap_defaults(eeprom_bootstrap_t *bs);

/**
 * Compute CRC-16/CCITT (poly 0x1021, init 0xFFFF) over a byte buffer.
 *
 * @param[in] data    Input bytes
 * @param[in] length  Number of bytes
 * @return 16-bit CRC
 */
uint16_t eeprom_crc16(const uint8_t *data, uint16_t length);

#endif /* EEPROM_LAYOUT_H */

/**
 * EEPROM Driver Implementation for FT24C02A
 * Orange Pi Zero 2W - I2C0 Interface
 */

#include "eeprom_driver.h"
#include "../../hal/i2c/i2c.h"
#include "../../config/pinout.h"
#include <string.h>
#include <unistd.h>

/* ===== EEPROM STATE ===== */
typedef struct {
    uint8_t initialized;
} eeprom_context_t;

static eeprom_context_t eeprom_ctx = {
    .initialized = 0,
};

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t eeprom_init(void)
{
    if (eeprom_ctx.initialized) {
        return HAL_OK;
    }

    /* Initialize I2C0 for EEPROM */
    i2c_config_t i2c_cfg = {
        .frequency = EEPROM_I2C_FREQ,
        .address_bits = 7,
    };
    
    if (i2c_init(I2C_BUS_0, &i2c_cfg) != HAL_OK) {
        return HAL_ERROR;
    }

    eeprom_ctx.initialized = 1;
    return HAL_OK;
}

hal_status_t eeprom_read(uint8_t address, uint8_t *buffer, uint16_t length)
{
    if (buffer == NULL || length == 0) {
        return HAL_INVALID_PARAM;
    }

    if (address + length > EEPROM_CAPACITY) {
        return HAL_INVALID_PARAM;
    }

    if (!eeprom_ctx.initialized) {
        return HAL_NOT_READY;
    }

    /* Send address byte, then read data */
    uint8_t addr[1] = {address};
    
    return i2c_write_read(I2C_BUS_0, EEPROM_ADDR,
                         addr, 1,
                         buffer, length);
}

hal_status_t eeprom_write(uint8_t address, const uint8_t *buffer, uint16_t length)
{
    if (buffer == NULL || length == 0) {
        return HAL_INVALID_PARAM;
    }

    if (address + length > EEPROM_CAPACITY) {
        return HAL_INVALID_PARAM;
    }

    if (!eeprom_ctx.initialized) {
        return HAL_NOT_READY;
    }

    /* FT24C02A page size is 8 bytes */
    /* Must write in page-aligned chunks */
    
    uint16_t bytes_written = 0;
    while (bytes_written < length) {
        uint16_t chunk_size = EEPROM_PAGE_SIZE;
        if (bytes_written + chunk_size > length) {
            chunk_size = length - bytes_written;
        }

        /* Check page boundary */
        if ((address + bytes_written) % EEPROM_PAGE_SIZE != 0) {
            /* Adjust for page boundary */
            chunk_size = EEPROM_PAGE_SIZE - ((address + bytes_written) % EEPROM_PAGE_SIZE);
        }

        /* Prepare write packet: [address, data...] */
        uint8_t write_packet[1 + 8];
        write_packet[0] = address + bytes_written;
        memcpy(&write_packet[1], &buffer[bytes_written], chunk_size);

        /* Send address + data */
        hal_status_t status = i2c_write(I2C_BUS_0, EEPROM_ADDR, 
                                       write_packet, 1 + chunk_size);
        if (status != HAL_OK) {
            return status;
        }

        /* Wait for EEPROM write cycle (~5ms) */
        usleep(5000);

        bytes_written += chunk_size;
    }

    return HAL_OK;
}

hal_status_t eeprom_deinit(void)
{
    if (!eeprom_ctx.initialized) {
        return HAL_OK;
    }

    i2c_deinit(I2C_BUS_0);
    eeprom_ctx.initialized = 0;
    return HAL_OK;
}

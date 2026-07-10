/**
 * SD Card Driver Implementation for push-pull 6-pin module
 * Orange Pi Zero 2W - SPI1 Interface
 */

#include "sdcard_driver.h"
#include "../../hal/spi/spi.h"
#include "../../hal/gpio/gpio.h"
#include "../../config/pinout.h"
#include <string.h>

/* ===== SD CARD COMMANDS ===== */
#define SD_CMD0             0    /* GO_IDLE_STATE */
#define SD_CMD1             1    /* SEND_OP_COND */
#define SD_CMD8             8    /* SEND_IF_COND */
#define SD_CMD9             9    /* SEND_CSD */
#define SD_CMD10            10   /* SEND_CID */
#define SD_CMD17            17   /* READ_SINGLE_BLOCK */
#define SD_CMD24            24   /* WRITE_SINGLE_BLOCK */
#define SD_CMD55            55   /* APP_CMD */
#define SD_ACMD41           41   /* SD_SEND_OP_COND (App command) */

#define SD_RESPONSE_TIMEOUT 1000

/* ===== SD CARD STATE ===== */
typedef struct {
    uint8_t initialized;
    uint32_t capacity;
} sdcard_context_t;

static sdcard_context_t sdcard_ctx = {
    .initialized = 0,
    .capacity = 0,
};

/* ===== LOCAL HELPER FUNCTIONS ===== */

/**
 * Send command to SD card
 */
static hal_status_t sdcard_send_command(uint8_t cmd, uint32_t arg)
{
    uint8_t cmd_packet[6];
    
    cmd_packet[0] = 0x40 | cmd;                /* Command byte */
    cmd_packet[1] = (arg >> 24) & 0xFF;        /* Argument MSB */
    cmd_packet[2] = (arg >> 16) & 0xFF;
    cmd_packet[3] = (arg >> 8) & 0xFF;
    cmd_packet[4] = arg & 0xFF;                /* Argument LSB */
    cmd_packet[5] = 0x95;                      /* CRC */

    return spi_write(SPI_BUS_1, SPI1_CS0, cmd_packet, 6);
}

/**
 * Read response from SD card
 */
static hal_status_t sdcard_read_response(uint8_t *response, uint32_t length)
{
    return spi_read(SPI_BUS_1, SPI1_CS0, response, length);
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t sdcard_init(void)
{
    if (sdcard_ctx.initialized) {
        return HAL_OK;
    }

    /* Initialize SPI1 for SD card */
    spi_config_t spi_cfg = {
        .frequency = SD_SPI_FREQ,
        .mode = SPI_MODE_0,
        .bits_per_word = 8,
        .bit_order = SPI_MSB_FIRST,
    };
    
    if (spi_init(SPI_BUS_1, &spi_cfg) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Initialize SD card via SPI */
    
    /* Send CMD0 (GO_IDLE_STATE) */
    sdcard_send_command(SD_CMD0, 0);
    
    uint8_t response[5];
    sdcard_read_response(response, 1);

    /* Send CMD8 (SEND_IF_COND) for voltage validation */
    sdcard_send_command(SD_CMD8, 0x1AA);
    sdcard_read_response(response, 5);

    /* Send CMD55 + ACMD41 (SD_SEND_OP_COND) */
    uint32_t timeout = 0;
    while (timeout < 1000) {
        sdcard_send_command(SD_CMD55, 0);
        sdcard_read_response(response, 1);
        
        sdcard_send_command(SD_ACMD41, 0x40000000);
        sdcard_read_response(response, 1);
        
        if ((response[0] & 0x80) == 0) {
            break;
        }
        timeout++;
    }

    sdcard_ctx.capacity = SD_SECTOR_SIZE * 1024;  /* Default 512KB sectors */
    sdcard_ctx.initialized = 1;
    
    return HAL_OK;
}

hal_status_t sdcard_read_sector(uint32_t sector, uint16_t count, uint8_t *buffer)
{
    if (buffer == NULL || count == 0) {
        return HAL_INVALID_PARAM;
    }

    if (!sdcard_ctx.initialized) {
        return HAL_NOT_READY;
    }

    /* Send CMD17 (READ_SINGLE_BLOCK) */
    sdcard_send_command(SD_CMD17, sector * SD_SECTOR_SIZE);
    
    uint8_t response[1];
    sdcard_read_response(response, 1);

    /* Wait for data token */
    uint8_t token = 0xFF;
    for (int i = 0; i < SD_RESPONSE_TIMEOUT; i++) {
        sdcard_read_response(&token, 1);
        if (token == 0xFE) {
            break;
        }
    }

    if (token != 0xFE) {
        return HAL_TIMEOUT;
    }

    /* Read sector data */
    sdcard_read_response(buffer, SD_SECTOR_SIZE);

    /* Read CRC (2 bytes) */
    uint8_t crc[2];
    sdcard_read_response(crc, 2);

    return HAL_OK;
}

hal_status_t sdcard_write_sector(uint32_t sector, uint16_t count, const uint8_t *buffer)
{
    if (buffer == NULL || count == 0) {
        return HAL_INVALID_PARAM;
    }

    if (!sdcard_ctx.initialized) {
        return HAL_NOT_READY;
    }

    /* Send CMD24 (WRITE_SINGLE_BLOCK) */
    sdcard_send_command(SD_CMD24, sector * SD_SECTOR_SIZE);

    uint8_t response[1];
    sdcard_read_response(response, 1);

    /* Send data token */
    uint8_t token = 0xFE;
    spi_write(SPI_BUS_1, SPI1_CS0, &token, 1);

    /* Write sector data */
    spi_write(SPI_BUS_1, SPI1_CS0, buffer, SD_SECTOR_SIZE);

    /* Send dummy CRC */
    uint8_t crc[2] = {0xFF, 0xFF};
    spi_write(SPI_BUS_1, SPI1_CS0, crc, 2);

    /* Wait for write completion */
    uint8_t status = 0xFF;
    for (int i = 0; i < SD_RESPONSE_TIMEOUT && status == 0xFF; i++) {
        sdcard_read_response(&status, 1);
    }

    return HAL_OK;
}

hal_status_t sdcard_get_capacity(uint32_t *capacity)
{
    if (capacity == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (!sdcard_ctx.initialized) {
        return HAL_NOT_READY;
    }

    *capacity = sdcard_ctx.capacity;
    return HAL_OK;
}

hal_status_t sdcard_deinit(void)
{
    if (!sdcard_ctx.initialized) {
        return HAL_OK;
    }

    spi_deinit(SPI_BUS_1);
    sdcard_ctx.initialized = 0;
    return HAL_OK;
}

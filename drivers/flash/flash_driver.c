/**
 * SPI Flash Driver Implementation for W25Q40
 * Orange Pi Zero 2W - SPI2 Interface
 */

#include "flash_driver.h"
#include "../../hal/spi/spi.h"
#include "../../hal/gpio/gpio.h"
#include "../../config/pinout.h"
#include <string.h>
#include <unistd.h>

/* ===== W25Q40 COMMANDS ===== */
#define W25Q_CMD_READ_ID       0x9F  /* Read JEDEC ID */
#define W25Q_CMD_READ_STATUS   0x05  /* Read Status Register */
#define W25Q_CMD_READ_DATA     0x03  /* Read Data */
#define W25Q_CMD_PAGE_WRITE    0x02  /* Page Program */
#define W25Q_CMD_SECTOR_ERASE  0x20  /* Sector Erase (4 KiB) */
#define W25Q_CMD_CHIP_ERASE    0xC7  /* Chip Erase */
#define W25Q_CMD_WRITE_ENABLE  0x06  /* Write Enable */
#define W25Q_CMD_WRITE_DISABLE 0x04  /* Write Disable */

#define W25Q_STATUS_BUSY       0x01

/* ===== FLASH STATE ===== */
typedef struct {
    uint8_t initialized;
    uint32_t capacity;
} flash_context_t;

static flash_context_t flash_ctx = {
    .initialized = 0,
    .capacity = FLASH_CAPACITY,
};

/* ===== LOCAL HELPER FUNCTIONS ===== */

/**
 * Wait for flash to be ready
 */
static hal_status_t flash_wait_ready(void)
{
    uint8_t status;
    uint32_t timeout = 10000;

    while (timeout-- > 0) {
        uint8_t cmd = W25Q_CMD_READ_STATUS;
        uint8_t status_byte;
        
        hal_status_t result = spi_transfer(SPI_BUS_2, SPI2_CS0,
                                          &cmd, 1,
                                          &status_byte, 1);
        
        if (result != HAL_OK) {
            return result;
        }

        if ((status_byte & W25Q_STATUS_BUSY) == 0) {
            return HAL_OK;
        }
        usleep(100);
    }

    return HAL_TIMEOUT;
}

/**
 * Send write enable command
 */
static hal_status_t flash_write_enable(void)
{
    uint8_t cmd = W25Q_CMD_WRITE_ENABLE;
    return spi_write(SPI_BUS_2, SPI2_CS0, &cmd, 1);
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t flash_init(void)
{
    if (flash_ctx.initialized) {
        return HAL_OK;
    }

    /* Initialize SPI2 for Flash */
    spi_config_t spi_cfg = {
        .frequency = FLASH_SPI_FREQ,
        .mode = SPI_MODE_0,
        .bits_per_word = 8,
        .bit_order = SPI_MSB_FIRST,
    };
    
    if (spi_init(SPI_BUS_2, &spi_cfg) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Verify JEDEC ID */
    uint8_t jedec_id[3];
    hal_status_t status = flash_get_jedec_id(jedec_id);
    if (status != HAL_OK) {
        return status;
    }

    /* Check if JEDEC ID matches W25Q40 */
    uint32_t id = (jedec_id[0] << 16) | (jedec_id[1] << 8) | jedec_id[2];
    if (id != FLASH_JEDEC_ID) {
        return HAL_ERROR;  /* Unexpected flash JEDEC ID */
    }

    flash_ctx.initialized = 1;
    return HAL_OK;
}

hal_status_t flash_read(uint32_t address, uint8_t *buffer, uint32_t length)
{
    if (buffer == NULL || length == 0 || address + length > FLASH_CAPACITY) {
        return HAL_INVALID_PARAM;
    }

    if (!flash_ctx.initialized) {
        return HAL_NOT_READY;
    }

    /* Read data command + 24-bit address + data */
    uint8_t cmd_packet[4 + 256];  /* Max page size is 256 bytes */
    
    cmd_packet[0] = W25Q_CMD_READ_DATA;
    cmd_packet[1] = (address >> 16) & 0xFF;
    cmd_packet[2] = (address >> 8) & 0xFF;
    cmd_packet[3] = address & 0xFF;

    return spi_transfer(SPI_BUS_2, SPI2_CS0, 
                       cmd_packet, 4,
                       buffer, length);
}

hal_status_t flash_write(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    if (buffer == NULL || length == 0 || length > FLASH_PAGE_SIZE) {
        return HAL_INVALID_PARAM;
    }

    if (address + length > FLASH_CAPACITY) {
        return HAL_INVALID_PARAM;
    }

    if (!flash_ctx.initialized) {
        return HAL_NOT_READY;
    }

    /* Enable write */
    flash_write_enable();

    /* Wait for ready */
    hal_status_t status = flash_wait_ready();
    if (status != HAL_OK) {
        return status;
    }

    /* Page write command + 24-bit address + data */
    uint8_t cmd_packet[4];
    cmd_packet[0] = W25Q_CMD_PAGE_WRITE;
    cmd_packet[1] = (address >> 16) & 0xFF;
    cmd_packet[2] = (address >> 8) & 0xFF;
    cmd_packet[3] = address & 0xFF;

    spi_write(SPI_BUS_2, SPI2_CS0, cmd_packet, 4);
    spi_write(SPI_BUS_2, SPI2_CS0, buffer, length);

    /* Wait for write to complete */
    return flash_wait_ready();
}

hal_status_t flash_erase_sector(uint32_t address)
{
    if (address % FLASH_SECTOR_SIZE != 0) {
        return HAL_INVALID_PARAM;  /* Must be sector-aligned */
    }

    if (address >= FLASH_CAPACITY) {
        return HAL_INVALID_PARAM;
    }

    if (!flash_ctx.initialized) {
        return HAL_NOT_READY;
    }

    /* Enable write */
    flash_write_enable();

    /* Wait for ready */
    hal_status_t status = flash_wait_ready();
    if (status != HAL_OK) {
        return status;
    }

    /* Sector erase command + 24-bit address */
    uint8_t cmd_packet[4];
    cmd_packet[0] = W25Q_CMD_SECTOR_ERASE;
    cmd_packet[1] = (address >> 16) & 0xFF;
    cmd_packet[2] = (address >> 8) & 0xFF;
    cmd_packet[3] = address & 0xFF;

    spi_write(SPI_BUS_2, SPI2_CS0, cmd_packet, 4);

    /* Wait for erase to complete */
    return flash_wait_ready();
}

hal_status_t flash_erase_all(void)
{
    if (!flash_ctx.initialized) {
        return HAL_NOT_READY;
    }

    /* Enable write */
    flash_write_enable();

    /* Wait for ready */
    hal_status_t status = flash_wait_ready();
    if (status != HAL_OK) {
        return status;
    }

    /* Chip erase command */
    uint8_t cmd = W25Q_CMD_CHIP_ERASE;
    spi_write(SPI_BUS_2, SPI2_CS0, &cmd, 1);

    /* Wait for erase to complete (can take ~30ms) */
    return flash_wait_ready();
}

hal_status_t flash_get_jedec_id(uint8_t *jedec_id)
{
    if (jedec_id == NULL) {
        return HAL_INVALID_PARAM;
    }

    /* Read JEDEC ID command */
    uint8_t cmd = W25Q_CMD_READ_ID;
    return spi_transfer(SPI_BUS_2, SPI2_CS0,
                       &cmd, 1,
                       jedec_id, 3);
}

hal_status_t flash_deinit(void)
{
    if (!flash_ctx.initialized) {
        return HAL_OK;
    }

    spi_deinit(SPI_BUS_2);
    flash_ctx.initialized = 0;
    return HAL_OK;
}

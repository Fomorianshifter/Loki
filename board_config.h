#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* AUTO-GENERATED FILE - do not edit directly.
 * Source: config.toml
 * Generator: tools/gen_config.py
 */

/* ===== BOARD CONFIGURATION ===== */
#define BOARD_NAME                   "Raspberry Pi Loki"
#define BOARD_VERSION                "1.0"
#define BOARD_MODEL                  "RPI_RS485_HAT"
#define POWER_INPUT_VOLTAGE          5.0
#define POWER_LOGIC_VOLTAGE          3.3
#define LDO_ENABLE_DELAY_MS          10
#define TFT_WIDTH                    480
#define TFT_HEIGHT                   320
#define TFT_SPI_FREQ                 40000000
#define TFT_TYPE                     "ILI9488"
#define TFT_COLOR_DEPTH              16
#define TFT_ROTATION                 0
#define TFT_BRIGHTNESS               100
#define SD_SPI_FREQ                  25000000
#define SD_SECTOR_SIZE               512
#define SD_FAST_MODE                 1
#define FLASH_IC_TYPE                "W25Q40"
#define FLASH_CAPACITY               524288
#define FLASH_PAGE_SIZE              256
#define FLASH_SECTOR_SIZE            4096
#define FLASH_SPI_FREQ               20000000
#define EEPROM_IC_TYPE               "FT24C02A"
#define EEPROM_CAPACITY              256
#define EEPROM_PAGE_SIZE             8
#define EEPROM_I2C_FREQ              100000
#define UART1_BAUD_RATE              115200
#define UART1_DATA_BITS              8
#define UART1_STOP_BITS              1
#define UART1_RX_BUFFER_SIZE         256
#define UART1_TX_BUFFER_SIZE         256
#define UART1_RS485_ENABLED          0
#define UART1_RS485_TURNAROUND_US    0
#define DECAP_BULK_UF                10
#define DECAP_CERAMIC_UF             0.1
#define I2C_PULLUP_OHM               4700
#define ENABLE_ERROR_LOGGING         1
#define ERROR_LOG_BUFFER_SIZE        256
#define INIT_DELAY_TFT               100
#define INIT_DELAY_FLASH             10
#define INIT_DELAY_EEPROM            5

/* ===== HEX CONSTANTS ===== */
#define FLASH_JEDEC_ID               0xEF4013
#define EEPROM_I2C_ADDR              0x50

/* ===== RAW CONSTANTS ===== */
#define UART1_PARITY                 UART_PARITY_NONE
#define UART1_RS485_DE_GPIO          RS485_DE_GPIO

#endif /* BOARD_CONFIG_H */

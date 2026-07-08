#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/**
 * Orange Pi Zero 2W Loki Board Configuration
 * Defines board-level settings for all subsystems
 */

/* ===== BOARD IDENTIFICATION ===== */
#define BOARD_NAME        "Orange Pi Zero 2W Loki"
#define BOARD_VERSION     "1.0"
#define BOARD_MODEL       "OPI_ZERO_2W"

/* ===== POWER CONFIGURATION ===== */
#define POWER_INPUT_VOLTAGE     5.0  /* 5V USB input from Flipper */
#define POWER_LOGIC_VOLTAGE     3.3  /* 3.3V logic level */
#define LDO_ENABLE_DELAY_MS     10   /* LDO startup delay */

/* ===== TFT DISPLAY CONFIGURATION ===== */
#define TFT_WIDTH         480   /* Pixels */
#define TFT_HEIGHT        320   /* Pixels */
#define TFT_SPI_FREQ      40000000  /* 40 MHz SPI frequency */
#define TFT_TYPE          "ILI9488"  /* Display controller IC */
#define TFT_COLOR_DEPTH   16    /* Bits per pixel */
#define TFT_ROTATION      0     /* 0=Normal, 1=90°, 2=180°, 3=270° */
#define TFT_BRIGHTNESS    100   /* Default brightness (0-100%) */

/* ===== SD CARD CONFIGURATION ===== */
#define SD_SPI_FREQ       25000000  /* 25 MHz SPI frequency */
#define SD_SECTOR_SIZE    512   /* Bytes */
#define SD_FAST_MODE      1     /* Enable fast SPI after init */

/* ===== LOKI CREDITS FLASH CONFIGURATION ===== */
#define FLASH_IC_TYPE     "W25Q40"  /* 4 Megabit SPI Flash */
#define FLASH_CAPACITY    524288    /* 512 KiB in bytes */
#define FLASH_PAGE_SIZE   256    /* Bytes */
#define FLASH_SECTOR_SIZE 4096   /* Bytes */
#define FLASH_SPI_FREQ    20000000  /* 20 MHz SPI frequency */
#define FLASH_JEDEC_ID    0xEF4013  /* W25Q40 JEDEC ID */

/* ===== EEPROM CONFIGURATION ===== */
#define EEPROM_IC_TYPE    "FT24C02A"   /* EEPROM model */
#define EEPROM_CAPACITY   256   /* Bytes */
#define EEPROM_PAGE_SIZE  8     /* Bytes */
#define EEPROM_I2C_ADDR   0x50  /* I2C 7-bit address */
#define EEPROM_I2C_FREQ   100000    /* 100 kHz standard mode */

/* ===== UART CONFIGURATION ===== */
#define UART1_BAUD_RATE       115200          /* Standard baud rate              */
#define UART1_DATA_BITS       8
#define UART1_STOP_BITS       1
#define UART1_PARITY          UART_PARITY_NONE
#define UART1_RX_BUFFER_SIZE  256
#define UART1_TX_BUFFER_SIZE  256

/* Device path — /dev/serial0 is the Pi's primary UART (symlink)            */
/* Override at compile time: make CFLAGS+=-DUART1_DEVICE_PATH='"/dev/ttyS0"' */
#ifndef UART1_DEVICE_PATH
#define UART1_DEVICE_PATH     "/dev/serial0"
#endif

/* RS-485 mode (0 = disabled; set to 1 to enable for RS-485 HAT)            */
#define UART1_RS485_ENABLED          0

/* BCM GPIO pin used as DE/RE when kernel auto-direction is unavailable.     */
/* Set rs485_de_gpio_fallback = 1 in uart_config_t to activate.             */
/* Common HAT wiring: GPIO4 (pin 7) or GPIO18 (pin 12) — verify your HAT.  */
#define UART1_RS485_DE_GPIO          RS485_DE_GPIO

/* Microseconds between tcdrain() and DE de-assertion (0 = none)            */
#define UART1_RS485_TURNAROUND_US    0

/* ===== DECOUPLING CAPACITOR VALUES ===== */
#define DECAP_BULK_UF     10    /* 10 µF bulk capacitors */
#define DECAP_CERAMIC_UF  0.1   /* 0.1 µF ceramic capacitors */

/* ===== PULL-UP RESISTORS ===== */
#define I2C_PULLUP_OHM    4700  /* 4.7 kΩ for I2C SDA/SCL */

/* ===== ERROR HANDLING ===== */
#define ENABLE_ERROR_LOGGING  1
#define ERROR_LOG_BUFFER_SIZE 256

/* ===== TIMING CONSTANTS (ms) ===== */
#define INIT_DELAY_TFT    100   /* TFT initialization delay */
#define INIT_DELAY_FLASH  10    /* Flash initialization delay */
#define INIT_DELAY_EEPROM 5     /* EEPROM initialization delay */

#endif /* BOARD_CONFIG_H */

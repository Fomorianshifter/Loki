/**
 * Orange Pi Zero 2W GPIO Pinout for Loki
 * Using 40-pin header GPIO numbering
 */

#ifndef PINOUT_OPI_H
#define PINOUT_OPI_H

#ifdef BOARD_ORANGE_PI_ZERO_2W

/* ===== GPIO PIN DEFINITIONS ===== */
#define GPIO_TFT_DC       18    /* TFT Data/Command control */
#define GPIO_TFT_RST      22    /* TFT Reset */
#define GPIO_TFT_BL       7     /* TFT Backlight (PWM capable) */

/* ===== SPI0 PINS (TFT Display) ===== */
#define SPI0_SCK          23    /* SPI0 Serial Clock */
#define SPI0_MOSI         19    /* SPI0 Master Out Slave In */
#define SPI0_MISO         21    /* SPI0 Master In Slave Out */
#define SPI0_CS0          24    /* SPI0 Chip Select 0 (TFT) */
#define SPI0_CS1          26    /* SPI0 Chip Select 1 (optional) */

/* ===== SPI1 PINS (SD Card) ===== */
#define SPI1_SCK          29    /* SPI1 Serial Clock */
#define SPI1_MOSI         31    /* SPI1 Master Out Slave In */
#define SPI1_MISO         33    /* SPI1 Master In Slave Out */
#define SPI1_CS0          32    /* SPI1 Chip Select 0 */

/* ===== SPI2 PINS (Flash Memory) ===== */
#define SPI2_SCK          13    /* SPI2 Serial Clock */
#define SPI2_MOSI         11    /* SPI2 Master Out Slave In */
#define SPI2_MISO         12    /* SPI2 Master In Slave Out */
#define SPI2_CS0          15    /* SPI2 Chip Select 0 */

/* ===== I2C0 PINS (EEPROM) ===== */
#define I2C0_SDA          3     /* I2C Data */
#define I2C0_SCL          5     /* I2C Clock */

/* ===== I2C1 PINS (Optional Expansion) ===== */
#define I2C1_SDA          27    /* I2C1 Data */
#define I2C1_SCL          28    /* I2C1 Clock */

/* ===== UART1 PINS (Flipper Zero) ===== */
#define UART1_TX          8     /* UART1 Transmit */
#define UART1_RX          10    /* UART1 Receive */

/* ===== DEVICE ADDRESSES ===== */
#define EEPROM_ADDR       0x50  /* FT24C02A EEPROM */

/* ===== PWM CONFIGURATION ===== */
#define PWM_TFT_BACKLIGHT GPIO_TFT_BL
#define PWM_FREQ_DEFAULT  1000  /* 1 kHz */

#endif /* BOARD_ORANGE_PI_ZERO_2W */

#endif /* PINOUT_OPI_H */

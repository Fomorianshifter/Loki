#ifndef PINOUT_H
#define PINOUT_H

/**
 * Orange Pi Zero 2W Loki Board Pinout Definitions
 * Based on official wiring guide for complete system integration
 */

/* ===== GPIO PIN DEFINITIONS ===== */
#define GPIO_TFT_DC       18    /* TFT Data/Command control */
#define GPIO_TFT_RST      22    /* TFT Reset */
#define GPIO_TFT_BL       7     /* TFT Backlight (PWM capable) */

/* ===== SPI0 PINS (TFT Display) ===== */
#define SPI0_SCK          23    /* SPI0 Serial Clock */
#define SPI0_MOSI         19    /* SPI0 Master Out Slave In */
#define SPI0_MISO         21    /* SPI0 Master In Slave Out (optional) */
#define SPI0_CS0          24    /* SPI0 Chip Select 0 (TFT) */
#define SPI0_CS1          26    /* SPI0 Chip Select 1 (TFT Touch, optional) */

/* ===== SPI1 PINS (SD Card) ===== */
#define SPI1_SCK          29    /* SPI1 Serial Clock */
#define SPI1_MOSI         31    /* SPI1 Master Out Slave In */
#define SPI1_MISO         33    /* SPI1 Master In Slave Out */
#define SPI1_CS0          32    /* SPI1 Chip Select 0 (SD Card) */

/* ===== SPI2 PINS (Loki Credits Flash) ===== */
#define SPI2_SCK          13    /* SPI2 Serial Clock */
#define SPI2_MOSI         11    /* SPI2 Master Out Slave In */
#define SPI2_MISO         12    /* SPI2 Master In Slave Out */
#define SPI2_CS0          15    /* SPI2 Chip Select 0 (Flash) */

/* ===== I2C0 PINS (EEPROM) ===== */
#define I2C0_SDA          3     /* I2C Data */
#define I2C0_SCL          5     /* I2C Clock */

/* ===== UART1 PINS (Flipper Zero) ===== */
#define UART1_TX          8     /* UART1 Transmit to Flipper RX */
#define UART1_RX          10    /* UART1 Receive from Flipper TX */

/* ===== POWER PINS ===== */
#define PIN_5V_IN         2     /* 5V Input from Flipper USB (Pin 2 or 4) */
#define PIN_3V3_OUT       1     /* 3.3V Output from onboard LDO */
#define PIN_GND           6     /* Ground pins: 6, 9, 14, 20, 25, 30, 34, 39 */

/* ===== DEVICE I2C ADDRESSES ===== */
#define EEPROM_ADDR       0x50  /* FT24C02A EEPROM I2C Address */

/* ===== DEVICE SPI CS DEFINITIONS ===== */
#define TFT_CS            SPI0_CS0
#define SD_CS             SPI1_CS0
#define FLASH_CS          SPI2_CS0

/* ===== PWM CONFIGURATION ===== */
#define PWM_TFT_BACKLIGHT GPIO_TFT_BL
#define PWM_FREQ_DEFAULT  1000  /* 1 kHz backlight PWM */

#endif /* PINOUT_H */

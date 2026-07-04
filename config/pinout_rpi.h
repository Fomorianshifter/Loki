/**
 * Raspberry Pi 3/4/5 GPIO Pinout for Loki
 * Using BCM (Broadcom) numbering
 */

#ifndef PINOUT_RPI_H
#define PINOUT_RPI_H

#ifdef BOARD_RASPBERRY_PI

/* ===== GPIO PIN DEFINITIONS ===== */
#define GPIO_TFT_DC       23    /* TFT Data/Command control */
#define GPIO_TFT_RST      24    /* TFT Reset */
#define GPIO_TFT_BL       18    /* TFT Backlight (PWM capable) */

/* ===== SPI0 PINS (TFT Display) ===== */
#define SPI0_SCK          11    /* BCM 11 */
#define SPI0_MOSI         10    /* BCM 10 */
#define SPI0_MISO         9     /* BCM 9 */
#define SPI0_CS0          8     /* BCM 8 (CE0) */
#define SPI0_CS1          7     /* BCM 7 (CE1) */

/* ===== SPI1 PINS (SD Card / Storage) ===== */
#define SPI1_SCK          21    /* BCM 21 */
#define SPI1_MOSI         20    /* BCM 20 */
#define SPI1_MISO         19    /* BCM 19 */
#define SPI1_CS0          26    /* BCM 26 */

/* ===== I2C0 PINS (EEPROM & Sensors) ===== */
#define I2C0_SDA          2     /* BCM 2 */
#define I2C0_SCL          3     /* BCM 3 */

/* ===== I2C1 PINS (Optional Expansion) ===== */
#define I2C1_SDA          4     /* BCM 4 */
#define I2C1_SCL          17    /* BCM 17 */

/* ===== UART0 PINS (Flipper/Debug) ===== */
#define UART0_TX          14    /* BCM 14 (GPIO 14) */
#define UART0_RX          15    /* BCM 15 (GPIO 15) */

/* ===== UART5 PINS (Alternative) ===== */
#define UART5_TX          12    /* BCM 12 */
#define UART5_RX          13    /* BCM 13 */

/* ===== DEVICE ADDRESSES ===== */
#define EEPROM_ADDR       0x50  /* FT24C02A */

/* ===== PWM CONFIGURATION ===== */
#define PWM_TFT_BACKLIGHT GPIO_TFT_BL
#define PWM_FREQ_DEFAULT  1000  /* 1 kHz */

#endif /* BOARD_RASPBERRY_PI */

#endif /* PINOUT_RPI_H */

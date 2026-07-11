#ifndef PINOUT_H
#define PINOUT_H

/* AUTO-GENERATED FILE - do not edit directly.
 * Source: config.toml
 * Generator: tools/gen_config.py
 */

/* ===== PINOUT CONFIGURATION ===== */
#define GPIO_TFT_DC                  23
#define GPIO_TFT_RST                 22
#define GPIO_TFT_BL                  26
#define SPI0_SCK                     23
#define SPI0_MOSI                    19
#define SPI0_MISO                    21
#define SPI0_CS0                     24
#define SPI0_CS1                     26
#define SPI1_SCK                     29
#define SPI1_MOSI                    31
#define SPI1_MISO                    33
#define SPI1_CS0                     32
#define SPI2_SCK                     13
#define SPI2_MOSI                    11
#define SPI2_MISO                    12
#define SPI2_CS0                     15
#define I2C0_SDA                     3
#define I2C0_SCL                     5
#define UART1_TX                     8
#define UART1_RX                     10
#define RS485_DE_GPIO                25
#define PIN_5V_IN                    2
#define PIN_3V3_OUT                  1
#define PIN_GND                      6
#define PWM_FREQ_DEFAULT             1000

/* ===== HEX CONSTANTS ===== */
#define EEPROM_ADDR                  0x50

/* ===== RAW CONSTANTS ===== */
#define TFT_CS                       SPI0_CS0
#define SD_CS                        SPI1_CS0
#define FLASH_CS                     SPI2_CS0
#define PWM_TFT_BACKLIGHT            GPIO_TFT_BL

#endif /* PINOUT_H */

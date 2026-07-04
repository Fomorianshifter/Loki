/**
 * Device Configuration Table
 * Centralizes all device addresses and settings
 */

#ifndef DEVICES_H
#define DEVICES_H

#include "loki_config.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
   I2C DEVICE CONFIGURATION TABLE
   ============================================================================ */

typedef struct {
    const char *name;           /* Device name */
    uint8_t i2c_address;        /* 7-bit I2C address */
    uint8_t i2c_bus;            /* Bus number (0, 1, etc) */
    uint32_t i2c_speed_hz;      /* Operating speed */
    uint8_t enabled;            /* 1=enabled, 0=disabled */
    uint16_t probe_timeout_ms;  /* Max time to wait for response */
} i2c_device_config_t;

static const i2c_device_config_t i2c_devices[] = {
    /* Format: name, address, bus, speed, enabled, timeout */
    {"EEPROM",       EEPROM_I2C_ADDR,    0, 100000,  ENABLE_EEPROM,       100},
    {"BME680",       BME680_I2C_ADDR,    BME680_I2C_BUS,       100000,  ENABLE_BME680,       200},
    {"BMP280",       BMP280_I2C_ADDR,    BMP280_I2C_BUS,       100000,  ENABLE_BMP280,       200},
    {"MPU6050",      MPU6050_I2C_ADDR,   MPU6050_I2C_BUS,      400000,  ENABLE_MPU6050,      150},
    {"ADS1115",      ADS1115_I2C_ADDR,   ADS1115_I2C_BUS,      100000,  ENABLE_ADS1115,      100},
    {"DS3231_RTC",   DS3231_I2C_ADDR,    DS3231_I2C_BUS,       100000,  ENABLE_DS3231_RTC,   100},
    {"INA219",       INA219_I2C_ADDR,    INA219_I2C_BUS,       100000,  ENABLE_INA219,       100},
    {"PCA9685",      PCA9685_I2C_ADDR,   PCA9685_I2C_BUS,      100000,  ENABLE_PCA9685,      150},
    {"VEML7700",     VEML7700_I2C_ADDR,  VEML7700_I2C_BUS,     100000,  ENABLE_VEML7700,     100},
    {"VL53L0X",      VL53L0X_I2C_ADDR,   VL53L0X_I2C_BUS,      400000,  ENABLE_VL53L0X,      150},
    {NULL, 0, 0, 0, 0, 0}  /* Sentinel */
};

/* ============================================================================
   SPI DEVICE CONFIGURATION TABLE
   ============================================================================ */

typedef struct {
    const char *name;           /* Device name */
    uint8_t spi_bus;            /* Bus number (0, 1, 2, 3) */
    uint32_t cs_pin;            /* Chip select GPIO pin */
    uint32_t frequency_hz;      /* SPI clock frequency */
    uint8_t mode;               /* SPI mode (0-3) */
    uint8_t enabled;            /* 1=enabled, 0=disabled */
} spi_device_config_t;

static const spi_device_config_t spi_devices[] = {
    /* Format: name, bus, cs_pin, frequency, mode, enabled */
    {"TFT_Display",   0, SPI0_CS0,  SPI0_FREQ_HZ,  0,  ENABLE_TFT_DISPLAY},
    {"SDCard",        1, SPI1_CS0,  SPI1_FREQ_HZ,  0,  ENABLE_SDCARD},
    {"Flash_Memory",  2, SPI2_CS0,  SPI2_FREQ_HZ,  0,  ENABLE_FLASH_MEMORY},
    {"NRF24L01",      3, NRF24_CS_PIN, SPI3_FREQ_HZ, 0, ENABLE_NRF24L01},
    {"MAX7219",       3, MAX7219_CS_PIN, SPI3_FREQ_HZ, 0, ENABLE_MAX7219},
    {NULL, 0, 0, 0, 0, 0}  /* Sentinel */
};

/* ============================================================================
   HELPER FUNCTIONS
   ============================================================================ */

/**
 * Find I2C device configuration by address
 */
static inline const i2c_device_config_t* i2c_get_device_config(uint8_t addr)
{
    for (int i = 0; i2c_devices[i].name != NULL; i++) {
        if (i2c_devices[i].i2c_address == addr) {
            return &i2c_devices[i];
        }
    }
    return NULL;
}

/**
 * Find SPI device configuration by name
 */
static inline const spi_device_config_t* spi_get_device_config(const char *name)
{
    for (int i = 0; spi_devices[i].name != NULL; i++) {
        if (__builtin_strcmp(spi_devices[i].name, name) == 0) {
            return &spi_devices[i];
        }
    }
    return NULL;
}

#endif /* DEVICES_H */

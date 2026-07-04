/**
 * Device Configuration Auto-Generator
 * Automatically enables/disables driver files based on loki_config.h
 * This file is included by the build system to define compilation flags
 */

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "loki_config.h"

/* ============================================================================
   AUTO-GENERATED DEVICE ENABLE FLAGS
   These control which driver source files are compiled
   ============================================================================ */

#if ENABLE_TFT_DISPLAY
    #define BUILD_DRIVER_TFT 1
#endif

#if ENABLE_SDCARD
    #define BUILD_DRIVER_SDCARD 1
#endif

#if ENABLE_FLASH_MEMORY
    #define BUILD_DRIVER_FLASH 1
#endif

#if ENABLE_EEPROM
    #define BUILD_DRIVER_EEPROM 1
#endif

#if ENABLE_FLIPPER_UART
    #define BUILD_DRIVER_FLIPPER 1
#endif

#if ENABLE_BME680
    #define BUILD_DRIVER_BME680 1
#endif

#if ENABLE_BMP280
    #define BUILD_DRIVER_BMP280 1
#endif

#if ENABLE_MPU6050
    #define BUILD_DRIVER_MPU6050 1
#endif

#if ENABLE_ADS1115
    #define BUILD_DRIVER_ADS1115 1
#endif

#if ENABLE_DS3231_RTC
    #define BUILD_DRIVER_DS3231 1
#endif

#if ENABLE_INA219
    #define BUILD_DRIVER_INA219 1
#endif

#if ENABLE_PCA9685
    #define BUILD_DRIVER_PCA9685 1
#endif

#if ENABLE_VEML7700
    #define BUILD_DRIVER_VEML7700 1
#endif

#if ENABLE_VL53L0X
    #define BUILD_DRIVER_VL53L0X 1
#endif

#if ENABLE_NRF24L01
    #define BUILD_DRIVER_NRF24L01 1
#endif

#if ENABLE_RFID_READER
    #define BUILD_DRIVER_RFID 1
#endif

#if ENABLE_MAX7219
    #define BUILD_DRIVER_MAX7219 1
#endif

/* ============================================================================
   PLATFORM-SPECIFIC INCLUDES
   ============================================================================ */

#ifdef BOARD_RASPBERRY_PI
    #include "pinout_rpi.h"
#elif defined(BOARD_ORANGE_PI_ZERO_2W)
    #include "pinout_opi.h"
#endif

#endif /* DEVICE_CONFIG_H */

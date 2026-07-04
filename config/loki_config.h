/**
 * Loki Global Configuration System
 * Single source of truth for all Loki settings
 * 
 * This file controls:
 * - Platform selection (Raspberry Pi / Orange Pi)
 * - Device enable/disable
 * - Bus speeds and timings
 * - Feature flags
 * - Debug options
 */

#ifndef LOKI_CONFIG_H
#define LOKI_CONFIG_H

#include <stdint.h>

/* ============================================================================
   SECTION 1: PLATFORM SELECTION
   ============================================================================ */

/**
 * PRIMARY PLATFORM - Choose ONE:
 *   - BOARD_RASPBERRY_PI   : Raspberry Pi 3/4/5
 *   - BOARD_ORANGE_PI_ZERO_2W : Orange Pi Zero 2W
 */
#define BOARD_RASPBERRY_PI
/* #define BOARD_ORANGE_PI_ZERO_2W */

/**
 * SECONDARY PLATFORM (optional network peer):
 * When enabled, allows Loki to communicate with another unit
 */
#define ENABLE_SECONDARY_PLATFORM 0
#define SECONDARY_PLATFORM_HOSTNAME "loki-secondary.local"
#define SECONDARY_PLATFORM_PORT 5555

/* ============================================================================
   SECTION 2: HARDWARE FEATURES
   ============================================================================ */

/* --- TFT Display (ILI9488 480x320) --- */
#define ENABLE_TFT_DISPLAY 1
#define TFT_BRIGHTNESS 75  /* 0-100% */
#define TFT_ROTATION 0     /* 0=normal, 1=90°, 2=180°, 3=270° */
#define TFT_REFRESH_RATE_HZ 60

/* --- SD Card Storage --- */
#define ENABLE_SDCARD 1
#define SDCARD_MAX_SIZE_MB 32000  /* Max supported: 2TB */

/* --- Flash Memory (Loki Credits) --- */
#define ENABLE_FLASH_MEMORY 1
#define FLASH_CAPACITY (512 * 1024)  /* 512 KB W25Q40 */
#define FLASH_SPI_FREQ 20000000      /* 20 MHz */
#define FLASH_JEDEC_ID 0xEF4013
#define FLASH_PAGE_SIZE 256
#define FLASH_SECTOR_SIZE (4 * 1024)  /* 4 KB */

/* --- EEPROM (Configuration Storage) --- */
#define ENABLE_EEPROM 1
#define EEPROM_I2C_ADDR 0x50
#define EEPROM_SIZE 256  /* Bytes */

/* --- Flipper Zero Integration --- */
#define ENABLE_FLIPPER_UART 1
#define FLIPPER_BAUD_RATE 115200
#define FLIPPER_UART_TIMEOUT_MS 5000

/* ============================================================================
   SECTION 3: I2C SENSOR DRIVERS
   ============================================================================ */

/* --- BME680: Temperature, Humidity, Pressure, Gas Sensor --- */
#define ENABLE_BME680 1
#define BME680_I2C_ADDR 0x77
#define BME680_I2C_BUS 0
#define BME680_SAMPLE_INTERVAL_MS 1000

/* --- BMP280: Barometric Pressure & Altitude --- */
#define ENABLE_BMP280 0
#define BMP280_I2C_ADDR 0x76
#define BMP280_I2C_BUS 0

/* --- MPU6050: 6-Axis Accelerometer + Gyroscope --- */
#define ENABLE_MPU6050 0
#define MPU6050_I2C_ADDR 0x68
#define MPU6050_I2C_BUS 0

/* --- ADS1115: 16-bit Analog-to-Digital Converter --- */
#define ENABLE_ADS1115 0
#define ADS1115_I2C_ADDR 0x48
#define ADS1115_I2C_BUS 0
#define ADS1115_SAMPLE_RATE 128  /* Hz */

/* --- DS3231: Precision Real-Time Clock --- */
#define ENABLE_DS3231_RTC 0
#define DS3231_I2C_ADDR 0x68
#define DS3231_I2C_BUS 0

/* --- INA219: Current & Power Monitor --- */
#define ENABLE_INA219 0
#define INA219_I2C_ADDR 0x40
#define INA219_I2C_BUS 0

/* --- PCA9685: 16-Channel PWM Servo Controller --- */
#define ENABLE_PCA9685 0
#define PCA9685_I2C_ADDR 0x40
#define PCA9685_I2C_BUS 0
#define PCA9685_FREQ_HZ 50  /* Servo frequency */

/* --- VEML7700: Ambient Light Sensor --- */
#define ENABLE_VEML7700 0
#define VEML7700_I2C_ADDR 0x10
#define VEML7700_I2C_BUS 0

/* --- VL53L0X: Time-of-Flight Distance Sensor --- */
#define ENABLE_VL53L0X 0
#define VL53L0X_I2C_ADDR 0x29
#define VL53L0X_I2C_BUS 0

/* ============================================================================
   SECTION 4: SPI DEVICE DRIVERS
   ============================================================================ */

/* --- SPI Bus Speeds --- */
#define SPI0_FREQ_HZ 40000000  /* TFT: 40 MHz */
#define SPI1_FREQ_HZ 25000000  /* SD Card: 25 MHz */
#define SPI2_FREQ_HZ 20000000  /* Flash: 20 MHz */
#define SPI3_FREQ_HZ 10000000  /* Expansion: 10 MHz */

/* --- NRF24L01+: 2.4GHz Wireless Transceiver --- */
#define ENABLE_NRF24L01 0
#define NRF24_SPI_BUS 3
#define NRF24_CS_PIN 25
#define NRF24_CE_PIN 24
#define NRF24_CHANNEL 76  /* 2476 MHz */
#define NRF24_POWER_DBM 0  /* 0 dBm max */

/* --- RFID Reader (125kHz) --- */
#define ENABLE_RFID_READER 0
#define RFID_I2C_ADDR 0x3A
#define RFID_I2C_BUS 0

/* --- MAX7219: LED Matrix Driver --- */
#define ENABLE_MAX7219 0
#define MAX7219_SPI_BUS 3
#define MAX7219_CS_PIN 26

/* ============================================================================
   SECTION 5: COMMUNICATION & NETWORKING
   ============================================================================ */

/* --- Network Configuration --- */
#define ENABLE_NETWORK 0
#define NETWORK_HOSTNAME "loki-dragon.local"
#define NETWORK_PORT 5555
#define ENABLE_REST_API 0
#define REST_API_PORT 8080
#define ENABLE_WEBSOCKET 0
#define WEBSOCKET_PORT 8081

/* --- MQTT (for IoT integration) --- */
#define ENABLE_MQTT 0
#define MQTT_BROKER_HOST "mosquitto.local"
#define MQTT_BROKER_PORT 1883
#define MQTT_TOPIC_PREFIX "loki/"

/* ============================================================================
   SECTION 6: STORAGE & PERSISTENCE
   ============================================================================ */

/* --- Credit System --- */
#define ENABLE_CREDIT_SYSTEM 1
#define CREDITS_START_VALUE 1000
#define CREDITS_STORAGE_LOCATION STORAGE_FLASH  /* FLASH or EEPROM */

/* --- Data Logging --- */
#define ENABLE_DATA_LOGGING 1
#define LOG_OUTPUT_UART 1      /* Send logs to UART */
#define LOG_OUTPUT_SDCARD 1    /* Write logs to SD card */
#define LOG_FILE_PATH "/mnt/sdcard/loki_logs.bin"
#define LOG_MAX_FILE_SIZE_MB 100
#define LOG_ROTATION_COUNT 10

/* --- Statistics --- */
#define ENABLE_STATISTICS 1
#define STATS_STORAGE_LOCATION STORAGE_EEPROM

/* ============================================================================
   SECTION 7: DEBUG & DEVELOPMENT
   ============================================================================ */

/* --- Logging Configuration --- */
#define LOG_LEVEL LOG_DEBUG          /* DEBUG, INFO, WARN, ERROR, CRITICAL */
#define ENABLE_COLOR_LOGS 1
#define ENABLE_TIMESTAMP_LOGS 1
#define ENABLE_SOURCE_LOCATION_LOGS 1

/* --- Debug Features --- */
#define ENABLE_MEMORY_LEAK_DETECTION 1
#define ENABLE_STACK_TRACE 1
#define ENABLE_PROFILING 0
#define ENABLE_SELFTEST 1
#define SELFTEST_ON_STARTUP 0

/* --- GPIO Test Pins (for oscilloscope/logic analyzer) --- */
#define ENABLE_GPIO_TEST_PINS 0
#define GPIO_TEST_PIN_1 17
#define GPIO_TEST_PIN_2 27

/* ============================================================================
   SECTION 8: FEATURE FLAGS
   ============================================================================ */

#define ENABLE_ANIMATIONS 1
#define ENABLE_SOUND 0  /* Requires audio module */
#define ENABLE_MOOD_SYSTEM 1
#define ENABLE_GESTURE_RECOGNITION 0
#define ENABLE_MACHINE_LEARNING 0
#define ENABLE_OTA_UPDATES 0

/* ============================================================================
   SECTION 9: PERFORMANCE TUNING
   ============================================================================ */

#define STACK_SIZE (128 * 1024)      /* 128 KB */
#define HEAP_SIZE (2 * 1024 * 1024)  /* 2 MB */
#define THREAD_POOL_SIZE 4
#define MAX_CONCURRENT_CONNECTIONS 10

/* ============================================================================
   SECTION 10: BUILD CONFIGURATION
   ============================================================================ */

#define OPTIMIZE_FOR_SIZE 0    /* 1=yes (smaller binary), 0=speed */
#define ENABLE_ASSERTIONS 1
#define ENABLE_WARNINGS_AS_ERRORS 1
#define VERSION "1.0.0"
#define BUILD_TIMESTAMP __TIMESTAMP__

/* ============================================================================
   VALIDATION & DERIVED CONSTANTS
   ============================================================================ */

/* Verify platform selection */
#ifdef BOARD_RASPBERRY_PI
    #define BOARD_NAME "Raspberry Pi"
    #define PLATFORM_RPI 1
#elif defined(BOARD_ORANGE_PI_ZERO_2W)
    #define BOARD_NAME "Orange Pi Zero 2W"
    #define PLATFORM_OPI 1
#else
    #error "ERROR: No platform board selected! Define BOARD_RASPBERRY_PI or BOARD_ORANGE_PI_ZERO_2W"
#endif

/* Storage location enum */
#define STORAGE_EEPROM 0
#define STORAGE_FLASH  1
#define STORAGE_SDCARD 2

/* Log levels */
#define LOG_DEBUG    0
#define LOG_INFO     1
#define LOG_WARN     2
#define LOG_ERROR    3
#define LOG_CRITICAL 4

#endif /* LOKI_CONFIG_H */

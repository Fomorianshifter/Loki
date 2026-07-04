/**
 * Platform Detection and Configuration
 * Supports: Raspberry Pi 3/4/5, Orange Pi Zero 2W
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

/* ===== PLATFORM DETECTION ===== */
#if defined(__arm__) || defined(__aarch64__)
    #define PLATFORM_ARM 1
#else
    #error "Loki requires ARM platform"
#endif

/* Platform enum */
typedef enum {
    PLATFORM_UNKNOWN = 0,
    PLATFORM_RASPBERRY_PI_3 = 1,
    PLATFORM_RASPBERRY_PI_4 = 2,
    PLATFORM_RASPBERRY_PI_5 = 3,
    PLATFORM_ORANGE_PI_ZERO_2W = 4,
} platform_t;

/* ===== BOARD-SPECIFIC GPIO LIMITS ===== */
#if defined(BOARD_RASPBERRY_PI)
    #define GPIO_COUNT 28
    #define I2C_BUS_COUNT 2
    #define SPI_BUS_COUNT 2
    #define UART_PORT_COUNT 6
    #define BOARD_NAME "Raspberry Pi"
    #define BOARD_FAMILY_RPI 1
#elif defined(BOARD_ORANGE_PI_ZERO_2W)
    #define GPIO_COUNT 32
    #define I2C_BUS_COUNT 2
    #define SPI_BUS_COUNT 3
    #define UART_PORT_COUNT 8
    #define BOARD_NAME "Orange Pi Zero 2W"
    #define BOARD_FAMILY_OPI 1
#else
    #error "No platform board defined. Define BOARD_RASPBERRY_PI or BOARD_ORANGE_PI_ZERO_2W"
#endif

/* ===== PLATFORM-SPECIFIC PATHS ===== */
#define DEV_I2C_PATH "/dev/i2c-%d"
#define DEV_SPI_PATH "/dev/spidev%d.%d"
#define DEV_GPIO_PATH "/sys/class/gpio"
#define DEV_PWM_PATH "/sys/class/pwm"

/* ===== RUNTIME PLATFORM DETECTION ===== */
platform_t platform_detect(void);
const char* platform_name(platform_t platform);

#endif /* PLATFORM_H */

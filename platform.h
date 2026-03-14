#ifndef PLATFORM_H
#define PLATFORM_H

/**
 * @file platform.h
 * @brief Platform abstraction for cross-platform compatibility
 *
 * Defines platform constants and conditional compilation flags
 * to enable building Loki on different embedded platforms.
 */

// Platform definitions
#define PLATFORM_LINUX     1  // Linux-based SBCs (Orange Pi, Raspberry Pi)
#define PLATFORM_ESP32     2  // ESP32 microcontroller
#define PLATFORM_RP2040    3  // Raspberry Pi RP2040
#define PLATFORM_FLIPPER   4  // Flipper Zero

// Current target platform (set via Makefile or build flags)
#ifndef PLATFORM
#define PLATFORM PLATFORM_LINUX
#endif

// Platform-specific includes and macros
#if PLATFORM == PLATFORM_LINUX
    #include <unistd.h>
    #include <signal.h>
    #define DELAY_MS(ms) usleep((ms) * 1000)
    #define DELAY_US(us) usleep(us)
#elif PLATFORM == PLATFORM_ESP32
    // ESP32-specific includes would go here
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #define DELAY_MS(ms) vTaskDelay(pdMS_TO_TICKS(ms))
    #define DELAY_US(us) // Implement microsecond delay
#elif PLATFORM == PLATFORM_RP2040
    // RP2040-specific includes
    #include "pico/stdlib.h"
    #include "pico/time.h"
    #define DELAY_MS(ms) sleep_ms(ms)
    #define DELAY_US(us) sleep_us(us)
#endif

#endif // PLATFORM_H
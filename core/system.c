/**
 * @file system.c
 * @brief System Initialization and Management Implementation
 * Orange Pi Zero 2W Loki Board
 */

#include "system.h"
#include "../config/pinout.h"
#include "../config/board_config.h"
#include "../utils/log.h"
#include "../utils/memory.h"

/* Import HAL drivers */
#include "../hal/gpio/gpio.h"
#include "../hal/spi/spi.h"
#include "../hal/i2c/i2c.h"
#include "../hal/uart/uart.h"
#include "../hal/pwm/pwm.h"

/* Import device drivers */
#include "../drivers/tft/tft_driver.h"
#include "../drivers/sdcard/sdcard_driver.h"
#include "../drivers/flash/flash_driver.h"
#include "../drivers/eeprom/eeprom_driver.h"
#include "../drivers/flipper_uart/flipper_uart.h"

/* ===== SYSTEM STATE ===== */
typedef struct {
    uint8_t initialized;
    uint8_t gpio_ready;
    uint8_t tft_ready;
    uint8_t sdcard_ready;
    uint8_t flash_ready;
    uint8_t eeprom_ready;
    uint8_t flipper_ready;
} system_state_t;

static system_state_t system_state = {0};

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t system_init(void)
{
    if (system_state.initialized) {
        LOG_INFO("System already initialized");
        return HAL_OK;
    }

    LOG_INFO("╔════════════════════════════════════════╗");
    LOG_INFO("║  Initializing Loki Board - %s v%s    ║", BOARD_NAME, BOARD_VERSION);
    LOG_INFO("╚════════════════════════════════════════╝");

    /* Initialize memory tracking */
    memory_init();

    /* Initialize GPIO subsystem */
    LOG_INFO("Initializing GPIO subsystem...");
    if (gpio_init() != HAL_OK) {
        LOG_CRITICAL("GPIO initialization failed");
        return HAL_ERROR;
    }
    system_state.gpio_ready = 1;

    /* Initialize TFT Display */
    LOG_INFO("Initializing TFT Display (480×320 ILI9488)...");
    if (tft_init() != HAL_OK) {
        LOG_WARN("TFT initialization failed (continuing without display)");
    } else {
        system_state.tft_ready = 1;
        LOG_INFO("TFT initialized - clearing display");
        tft_clear();
        tft_set_brightness(TFT_BRIGHTNESS);
    }

    /* Initialize SD Card */
    LOG_INFO("Initializing SD Card (SPI1)...");
    if (sdcard_init() != HAL_OK) {
        LOG_WARN("SD Card initialization failed (continuing without SD)");
    } else {
        system_state.sdcard_ready = 1;
        LOG_INFO("SD Card initialized");
    }

    /* Initialize Flash Memory (Loki Credits) */
    LOG_INFO("Initializing SPI Flash (W25Q40)...");
    if (flash_init() != HAL_OK) {
        LOG_WARN("Flash initialization failed (continuing without flash)");
    } else {
        system_state.flash_ready = 1;
        LOG_INFO("SPI Flash initialized");
    }

    /* Initialize EEPROM */
    LOG_INFO("Initializing EEPROM (FT24C02A)...");
    if (eeprom_init() != HAL_OK) {
        LOG_WARN("EEPROM initialization failed (continuing without EEPROM)");
    } else {
        system_state.eeprom_ready = 1;
        LOG_INFO("EEPROM initialized");
    }

    /* Initialize Flipper UART */
    LOG_INFO("Initializing Flipper UART (115200 baud)...");
    if (flipper_uart_init() != HAL_OK) {
        LOG_WARN("Flipper UART initialization failed (continuing without Flipper)");
    } else {
        system_state.flipper_ready = 1;
        LOG_INFO("Flipper UART initialized");
    }

    system_state.initialized = 1;
    LOG_INFO("System initialization complete");
    system_print_status();

    return HAL_OK;
}

void system_print_status(void)
{
    LOG_INFO("╔════════════════════════════════════════╗");
    LOG_INFO("║       System Status Report             ║");
    LOG_INFO("╠════════════════════════════════════════╣");
    LOG_INFO("║ System Initialized:  %s", system_state.initialized ? "✓ YES" : "✗ NO");
    LOG_INFO("║ GPIO:                %s", system_state.gpio_ready ? "✓ OK" : "✗ FAILED");
    LOG_INFO("║ TFT Display:         %s", system_state.tft_ready ? "✓ OK" : "✗ FAILED");
    LOG_INFO("║ SD Card:             %s", system_state.sdcard_ready ? "✓ OK" : "✗ FAILED");
    LOG_INFO("║ Flash Memory:        %s", system_state.flash_ready ? "✓ OK" : "✗ FAILED");
    LOG_INFO("║ EEPROM:              %s", system_state.eeprom_ready ? "✓ OK" : "✗ FAILED");
    LOG_INFO("║ Flipper UART:        %s", system_state.flipper_ready ? "✓ OK" : "✗ FAILED");
    
#ifdef DEBUG
    LOG_INFO("║ Memory Usage:        %zu bytes", memory_get_usage());
#endif
    
    LOG_INFO("╚════════════════════════════════════════╝");
}

hal_status_t system_shutdown(void)
{
    LOG_INFO("System shutdown initiated");

    /* Deinitialize in reverse order */
    
    if (system_state.flipper_ready) {
        flipper_uart_deinit();
        system_state.flipper_ready = 0;
    }

    if (system_state.eeprom_ready) {
        eeprom_deinit();
        system_state.eeprom_ready = 0;
    }

    if (system_state.flash_ready) {
        flash_deinit();
        system_state.flash_ready = 0;
    }

    if (system_state.sdcard_ready) {
        sdcard_deinit();
        system_state.sdcard_ready = 0;
    }

    if (system_state.tft_ready) {
        tft_deinit();
        system_state.tft_ready = 0;
    }

    gpio_deinit();
    system_state.gpio_ready = 0;

    /* Print memory report in debug mode */
#ifdef DEBUG
    memory_report();
#endif
    
    memory_deinit();

    system_state.initialized = 0;
    LOG_INFO("System shutdown complete");
    log_deinit();
    
    return HAL_OK;
}

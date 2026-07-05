/**
 * @file main.c
 * @brief Loki portable Linux SBC interactive display system
 * 
 * Main entry point and example usage of the Loki board system.
 * Demonstrates hardware initialization, device testing, and communication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <strings.h>

#include "system.h"
#include "tft_driver.h"
#include "sdcard_driver.h"
#include "flash_driver.h"
#include "eeprom_driver.h"
#include "flipper_uart.h"
#include "log.h"
#include "memory.h"
#include "retry.h"
#include "runtime_config.h"
#include "setup_wizard.h"

/* ===== GLOBAL STATE ===== */
volatile sig_atomic_t should_exit = 0;

/* ===== SIGNAL HANDLERS ===== */
/**
 * @brief Signal handler for graceful shutdown
 */
static void handle_signal(int sig)
{
    LOG_WARN("Received signal %d, initiating shutdown...", sig);
    should_exit = 1;
}

static log_level_t log_level_from_string(const char *level)
{
    if (level == NULL) {
        return LOG_INFO;
    }

    if (strcasecmp(level, "debug") == 0) {
        return LOG_DEBUG;
    }
    if (strcasecmp(level, "info") == 0) {
        return LOG_INFO;
    }
    if (strcasecmp(level, "warn") == 0 || strcasecmp(level, "warning") == 0) {
        return LOG_WARN;
    }
    if (strcasecmp(level, "error") == 0) {
        return LOG_ERROR;
    }
    if (strcasecmp(level, "critical") == 0) {
        return LOG_CRITICAL;
    }

    return LOG_INFO;
}

/* ===== EXAMPLE: TFT DISPLAY TEST ===== */
/**
 * @brief Test TFT display functionality
 * 
 * Demonstrates display clearing and drawing colored rectangles.
 */
static void test_tft_display(void)
{
    LOG_INFO("Running TFT Display Test...");

    if (tft_clear() != HAL_OK) {
        LOG_ERROR("Failed to clear display");
        return;
    }

    /* Draw some patterns */
    tft_fill_rect(0, 0, 100, 100, RGB565(255, 0, 0));      /* Red square */
    tft_fill_rect(100, 0, 100, 100, RGB565(0, 255, 0));    /* Green square */
    tft_fill_rect(200, 0, 100, 100, RGB565(0, 0, 255));    /* Blue square */
    tft_fill_rect(300, 0, 180, 100, RGB565(255, 255, 0));  /* Yellow square */

    LOG_INFO("Display test complete");
}

/* ===== EXAMPLE: EEPROM READ/WRITE TEST ===== */
/**
 * @brief Test EEPROM read/write with verification
 * 
 * Writes test data to EEPROM, reads it back, and verifies integrity.
 */
static void test_eeprom(void)
{
    LOG_INFO("Running EEPROM Test...");

    uint8_t write_data[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    uint8_t read_data[8] = {0};

    /* Write to EEPROM with retry logic */
    hal_status_t status = RETRY(eeprom_write(0, write_data, 8), RETRY_BALANCED);
    
    if (status != HAL_OK) {
        LOG_ERROR("Failed to write EEPROM after retries");
        return;
    }
    LOG_INFO("Written 8 bytes to EEPROM address 0");

    sleep(1);  /* Wait for write to complete */

    /* Read from EEPROM */
    status = RETRY(eeprom_read(0, read_data, 8), RETRY_BALANCED);
    
    if (status != HAL_OK) {
        LOG_ERROR("Failed to read EEPROM");
        return;
    }

    /* Verify */
    LOG_INFO("Read from EEPROM: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
            read_data[0], read_data[1], read_data[2], read_data[3],
            read_data[4], read_data[5], read_data[6], read_data[7]);

    if (memcmp(write_data, read_data, 8) == 0) {
        LOG_INFO("✓ EEPROM test PASSED");
    } else {
        LOG_ERROR("✗ EEPROM test FAILED (data mismatch)");
    }
}

/* ===== EXAMPLE: FLASH MEMORY TEST ===== */
/**
 * @brief Test Flash memory JEDEC ID
 * 
 * Reads and verifies the JEDEC ID from the W25Q40 Flash chip.
 */
static void test_flash(void)
{
    LOG_INFO("Running Flash Memory Test...");

    uint8_t jedec_id[3];
    hal_status_t status = RETRY(flash_get_jedec_id(jedec_id), RETRY_BALANCED);
    
    if (status == HAL_OK) {
        LOG_INFO("Flash JEDEC ID: 0x%02X%02X%02X", jedec_id[0], jedec_id[1], jedec_id[2]);
        
        if ((jedec_id[0] << 16 | jedec_id[1] << 8 | jedec_id[2]) == FLASH_JEDEC_ID) {
            LOG_INFO("✓ Flash identification verified");
        } else {
            LOG_WARN("Flash JEDEC ID mismatch (expected: 0x%06X)", FLASH_JEDEC_ID);
        }
    } else {
        LOG_ERROR("Failed to read Flash JEDEC ID");
    }
}

/* ===== EXAMPLE: FLIPPER UART TEST ===== */
/**
 * @brief Test Flipper UART connectivity
 * 
 * Checks if Flipper is connected and responsive.
 */
static void test_flipper_communication(void)
{
    LOG_INFO("Running Flipper UART Test...");

    if (flipper_available() > 0) {
        LOG_INFO("✓ Flipper data available");
    } else {
        LOG_WARN("No Flipper data available (is Flipper connected?)");
    }
}

/* ===== MAIN APPLICATION ===== */
/**
 * @brief Main application entry point
 * 
 * Initializes the system, runs hardware tests, and waits for user input.
 * 
 * @param[in] argc Argument count
 * @param[in] argv Argument values
 * @return EXIT_SUCCESS on normal shutdown, EXIT_FAILURE on error
 */
int main(int argc, char *argv[])
{
    const char *config_path = getenv("LOKI_CONFIG_PATH");
    loki_runtime_config_t runtime_config;
    uint8_t created_default = 0;

    (void)argc;
    (void)argv;

    if (config_path == NULL || config_path[0] == '\0') {
        config_path = LOKI_CONFIG_PATH_DEFAULT;
    }

    /* Print banner */
    fprintf(stdout, "╔════════════════════════════════════════════════════╗\n");
    fprintf(stdout, "║     Loki - Portable Linux SBC Display System      ║\n");
    fprintf(stdout, "║  Default profile: Raspberry Pi Zero W (runtime)   ║\n");
    fprintf(stdout, "╚════════════════════════════════════════════════════╝\n\n");

    /* Initialize logging system */
    log_init();
    
    log_set_level(LOG_INFO);

    if (loki_config_load_or_create(config_path, &runtime_config, &created_default) != HAL_OK) {
        LOG_WARN("Failed to use config path '%s', falling back to '%s'",
                 config_path, LOKI_CONFIG_PATH_FALLBACK);
        config_path = LOKI_CONFIG_PATH_FALLBACK;
        if (loki_config_load_or_create(config_path, &runtime_config, &created_default) != HAL_OK) {
            LOG_CRITICAL("Unable to load/create runtime config");
            return EXIT_FAILURE;
        }
    }

    if (created_default || runtime_config.device.first_boot) {
        if (loki_setup_wizard_run(config_path, &runtime_config) != HAL_OK) {
            LOG_CRITICAL("Initial setup failed");
            return EXIT_FAILURE;
        }
    }

    log_set_level(log_level_from_string(runtime_config.logging.level));
    LOG_INFO("Using runtime config: %s", config_path);
    LOG_INFO("Board profile selected: %s", runtime_config.device.board_profile);

    /* Set up signal handlers for graceful shutdown */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* Initialize system */
    if (system_init() != HAL_OK) {
        LOG_CRITICAL("System initialization failed!");
        return EXIT_FAILURE;
    }

    tft_set_rotation(runtime_config.ui.rotation);
    tft_set_brightness(runtime_config.ui.brightness);

    /* Run hardware tests */
    LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    LOG_INFO("Running hardware tests...");
    LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    test_tft_display();
    sleep(1);

    test_flash();
    sleep(1);

    test_eeprom();
    sleep(1);

    test_flipper_communication();
    sleep(1);

    LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    /* Main loop */
    LOG_INFO("Entering main loop. Press Ctrl+C to exit.");
    LOG_INFO("Waiting for Flipper commands...\n");

    while (!should_exit) {
        /* Check for Flipper messages */
        if (flipper_available() > 0) {
            flipper_message_t msg = {0};
            
            if (flipper_receive_message(&msg, 100) == HAL_OK) {
                LOG_INFO("Received Flipper command: 0x%02X (length: %d)", 
                        msg.cmd, msg.length);

                /* Send acknowledgment */
                flipper_message_t ack = {
                    .cmd = FLIPPER_CMD_ACK,
                    .length = 1,
                    .payload = (uint8_t *)&msg.cmd,
                };
                flipper_send_message(&ack);

                /* Free payload if allocated */
                if (msg.payload != NULL) {
                    free(msg.payload);
                }
            }
        }

        sleep(1);  /* CPU-friendly polling */
    }

    /* Shutdown */
    LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    LOG_INFO("Initiating system shutdown...");
    system_shutdown();
    
    LOG_INFO("Loki system terminated successfully");
    fprintf(stdout, "\n✓ Goodbye!\n");
    
    return EXIT_SUCCESS;
}

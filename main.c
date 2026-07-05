/**
 * @file main.c
 * @brief Loki - Orange Pi Zero 2W Interactive Display System
 * 
 * Main entry point and example usage of the Loki board system.
 * Demonstrates hardware initialization, device testing, and communication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <string.h>

#include "system.h"
#include "tft_driver.h"
#include "sdcard_driver.h"
#include "flash_driver.h"
#include "eeprom_driver.h"
#include "flipper_uart.h"
#include "log.h"
#include "memory.h"
#include "retry.h"

/* ===== GLOBAL STATE ===== */
volatile sig_atomic_t should_exit = 0;

typedef struct {
    int safe_mode;
    int enable_credit_write;
} runtime_config_t;

static int parse_env_flag(const char *name, int default_value)
{
    const char *value = getenv(name);

    if (value == NULL || *value == '\0') {
        return default_value;
    }

    if (strcmp(value, "1") == 0 ||
        strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "yes") == 0 ||
        strcasecmp(value, "on") == 0) {
        return 1;
    }

    if (strcmp(value, "0") == 0 ||
        strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "no") == 0 ||
        strcasecmp(value, "off") == 0) {
        return 0;
    }

    LOG_WARN("Unrecognized %s value '%s'; using default %d", name, value, default_value);
    return default_value;
}

static runtime_config_t load_runtime_config(void)
{
    runtime_config_t config = {
        .safe_mode = parse_env_flag("SAFE_MODE", 1),
        .enable_credit_write = parse_env_flag("ENABLE_CREDIT_WRITE", 0),
    };

    if (config.safe_mode) {
        config.enable_credit_write = 0;
    }

    return config;
}

/* ===== SIGNAL HANDLERS ===== */
/**
 * @brief Signal handler for graceful shutdown
 */
static void handle_signal(int sig)
{
    LOG_WARN("Received signal %d, initiating shutdown...", sig);
    should_exit = 1;
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
    runtime_config_t runtime_config;

    (void)argc;
    (void)argv;

    /* Print banner */
    fprintf(stdout, "╔════════════════════════════════════════════════════╗\n");
    fprintf(stdout, "║        Loki - Orange Pi Zero 2W Display System    ║\n");
    fprintf(stdout, "║         Powered by Flipper Zero Integration       ║\n");
    fprintf(stdout, "╚════════════════════════════════════════════════════╝\n\n");

    /* Initialize logging system */
    log_init();
    runtime_config = load_runtime_config();
    
#if DEBUG
    log_set_level(LOG_DEBUG);
    LOG_DEBUG("Debug mode enabled");
#else
    log_set_level(LOG_INFO);
    LOG_INFO("Release mode");
#endif

    /* Set up signal handlers for graceful shutdown */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    LOG_INFO("Runtime safety: SAFE_MODE=%d, ENABLE_CREDIT_WRITE=%d",
             runtime_config.safe_mode, runtime_config.enable_credit_write);

    /* Initialize system */
    if (system_init() != HAL_OK) {
        LOG_CRITICAL("System initialization failed!");
        return EXIT_FAILURE;
    }

    /* Run hardware tests */
    LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    LOG_INFO("Running hardware tests...");
    LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    test_tft_display();
    sleep(1);

    test_flash();
    sleep(1);

    if (runtime_config.safe_mode || !runtime_config.enable_credit_write) {
        LOG_WARN("Skipping EEPROM write test. Set SAFE_MODE=0 and ENABLE_CREDIT_WRITE=1 to opt in.");
    } else {
        test_eeprom();
        sleep(1);
    }

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

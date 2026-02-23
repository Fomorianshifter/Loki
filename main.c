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
#include <string.h>

#include "core/system.h"
#include "drivers/tft/tft_driver.h"
#include "drivers/sdcard/sdcard_driver.h"
#include "drivers/flash/flash_driver.h"
#include "drivers/eeprom/eeprom_driver.h"
#include "drivers/flipper_uart/flipper_uart.h"
#include "utils/log.h"
#include "utils/memory.h"
#include "utils/retry.h"

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
    /* Print banner */
    fprintf(stdout, "╔════════════════════════════════════════════════════╗\n");
    fprintf(stdout, "║        Loki - Orange Pi Zero 2W Display System    ║\n");
    fprintf(stdout, "║         Powered by Flipper Zero Integration       ║\n");
    fprintf(stdout, "╚════════════════════════════════════════════════════╝\n\n");

    /* Initialize logging system */
    log_init();
    
#ifdef DEBUG
    log_set_level(LOG_DEBUG);
    LOG_DEBUG("Debug mode enabled");
#else
    log_set_level(LOG_INFO);
    LOG_INFO("Release mode");
#endif

    /* Set up signal handlers for graceful shutdown */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

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

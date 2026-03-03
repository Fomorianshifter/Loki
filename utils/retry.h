#ifndef RETRY_H
#define RETRY_H

/**
 * @file retry.h
 * @brief Error recovery and retry logic for bus operations
 * 
 * Provides automatic retry mechanisms for I2C, SPI, and UART
 * operations that may fail transiently.
 */

#include "../includes/types.h"

/* ===== RETRY CONFIGURATION ===== */

typedef struct {
    uint8_t max_attempts;      /* Maximum retry attempts (1 = no retry) */
    uint16_t initial_delay_ms; /* Initial delay between retries */
    uint8_t backoff_factor;    /* Delay multiplier on each retry (1.0x = no backoff) */
} retry_config_t;

/* Default retry strategies */
extern const retry_config_t RETRY_AGGRESSIVE;  /* 10 attempts, minimal delay */
extern const retry_config_t RETRY_BALANCED;    /* 5 attempts, moderate delay */
extern const retry_config_t RETRY_CONSERVATIVE; /* 2 attempts, long delay */
extern const retry_config_t RETRY_NONE;        /* No retry */

/* ===== RETRY WRAPPER MACROS ===== */

/**
 * @def RETRY
 * Execute function with automatic retry logic
 * 
 * @param[in] func Function to call (must return hal_status_t)
 * @param[in] config Retry configuration
 * @return HAL_OK on success, or last error code
 * 
 * Usage:
 *   status = RETRY(spi_write(SPI_BUS_0, pin, data, len), RETRY_BALANCED);
 */
#define RETRY(func, config) \
    retry_execute((func), (config), #func, __FILE__, __LINE__)

/**
 * @def RETRY_BLOCK
 * Execute block of code with retry logic
 * 
 * Usage:
 *   RETRY_BLOCK(RETRY_BALANCED) {
 *       status = spi_write(...);
 *       if (status != HAL_OK) break;
 *   }
 */

/* ===== CORE RETRY FUNCTION ===== */

/**
 * Execute function with automatic retries on failure
 * @param[in] result Result of function call
 * @param[in] config Retry configuration
 * @param[in] func_name Function name for logging
 * @param[in] file Source file
 * @param[in] line Source line
 * @return Final status code
 */
hal_status_t retry_execute(hal_status_t result, const retry_config_t config,
                           const char *func_name, const char *file, int line);

/**
 * Test if an error is retryable
 * @param[in] error Error code to test
 * @return 1 if retryable, 0 if permanent failure
 */
int is_retryable_error(hal_status_t error);

#endif /* RETRY_H */

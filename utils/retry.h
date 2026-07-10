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
#include <unistd.h>

/* ===== RETRY CONFIGURATION ===== */

typedef struct {
    uint8_t max_attempts;      /* Maximum retry attempts (1 = no retry) */
    uint16_t initial_delay_ms; /* Initial delay between retries */
    uint8_t backoff_factor;    /* Delay multiplier on each retry (1 = no backoff) */
} retry_config_t;

/* Default retry strategies */
extern const retry_config_t RETRY_AGGRESSIVE;   /* 10 attempts, minimal delay */
extern const retry_config_t RETRY_BALANCED;     /* 5 attempts, moderate delay */
extern const retry_config_t RETRY_CONSERVATIVE; /* 3 attempts, long delay */
extern const retry_config_t RETRY_NONE;         /* No retry */

/* ===== RETRY WRAPPER MACRO ===== */

/**
 * @def RETRY
 * Execute expr up to config.max_attempts times, re-evaluating on each attempt.
 *
 * Uses a GCC/Clang statement expression so expr is called again on every retry
 * (not just once with the result forwarded).
 *
 * @param[in] expr  Expression returning hal_status_t (e.g. a function call)
 * @param[in] config Retry configuration
 * @return HAL_OK on success, or the last error code
 *
 * Usage:
 *   status = RETRY(spi_write(SPI_BUS_0, pin, data, len), RETRY_BALANCED);
 */
#define RETRY(expr, config)                                                    \
    __extension__ ({                                                           \
        const retry_config_t _rc = (config);                                   \
        hal_status_t _r = (expr);                                              \
        if (_r != HAL_OK && is_retryable_error(_r)) {                          \
            uint32_t _d = (uint32_t)_rc.initial_delay_ms;                      \
            uint8_t _a;                                                        \
            for (_a = 1; _a < _rc.max_attempts; _a++) {                        \
                usleep(_d * 1000U);                                            \
                _d = (_d * _rc.backoff_factor < 5000U)                         \
                   ? _d * _rc.backoff_factor : 5000U;                          \
                _r = (expr);                                                   \
                if (_r == HAL_OK) break;                                       \
            }                                                                  \
        }                                                                      \
        _r;                                                                    \
    })

/* ===== HELPER FUNCTIONS ===== */

/**
 * Test if an error is retryable
 * @param[in] error Error code to test
 * @return 1 if retryable, 0 if permanent failure
 */
int is_retryable_error(hal_status_t error);

#endif /* RETRY_H */

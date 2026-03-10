/**
 * @file retry.c
 * @brief Retry logic implementation
 */

#include "retry.h"
#include "log.h"

/* ===== DEFAULT RETRY STRATEGIES ===== */

const retry_config_t RETRY_AGGRESSIVE = {
    .max_attempts = 10,
    .initial_delay_ms = 1,
    .backoff_factor = 2,
};

const retry_config_t RETRY_BALANCED = {
    .max_attempts = 5,
    .initial_delay_ms = 10,
    .backoff_factor = 2,
};

const retry_config_t RETRY_CONSERVATIVE = {
    .max_attempts = 3,
    .initial_delay_ms = 50,
    .backoff_factor = 2,
};

const retry_config_t RETRY_NONE = {
    .max_attempts = 1,
    .initial_delay_ms = 0,
    .backoff_factor = 1,
};

/* ===== PUBLIC IMPLEMENTATION ===== */

int is_retryable_error(hal_status_t error)
{
    /* These errors might succeed on retry */
    switch (error) {
        case HAL_TIMEOUT:
        case HAL_BUSY:
            return 1;
        
        /* These are permanent failures */
        case HAL_INVALID_PARAM:
        case HAL_NOT_SUPPORTED:
            return 0;
        
        /* Generic error might be transient */
        case HAL_ERROR:
        case HAL_NOT_READY:
            return 1;
        
        default:
            return 0;
    }
}

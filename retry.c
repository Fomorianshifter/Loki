/**
 * @file retry.c
 * @brief Retry logic implementation
 */

#include "retry.h"
#include "log.h"
#include <unistd.h>

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

hal_status_t retry_execute(hal_status_t result, const retry_config_t config,
                           const char *func_name, const char *file, int line)
{
    /* Success on first try */
    if (result == HAL_OK) {
        return HAL_OK;
    }

    /* Non-retryable error */
    if (!is_retryable_error(result)) {
        LOG_ERROR("[%s() at %s:%d] Non-retryable error: %d", func_name, file, line, result);
        return result;
    }

    /* Perform retries */
    uint32_t delay_ms = config.initial_delay_ms;
    
    for (uint8_t attempt = 1; attempt < config.max_attempts; attempt++) {
        LOG_WARN("[%s() at %s:%d] Retrying (%d/%d)... waiting %u ms",
                func_name, file, line, attempt, config.max_attempts, delay_ms);
        
        /* Wait before retry */
        usleep(delay_ms * 1000);
        
        /* Exponential backoff */
        delay_ms = (delay_ms * config.backoff_factor);
        if (delay_ms > 5000) {
            delay_ms = 5000;  /* Cap at 5 seconds */
        }
    }

    LOG_ERROR("[%s() at %s:%d] Failed after %d attempts", 
             func_name, file, line, config.max_attempts);
    
    return result;
}

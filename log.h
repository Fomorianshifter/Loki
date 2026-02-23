#ifndef LOG_H
#define LOG_H

/**
 * @file log.h
 * @brief Centralized logging framework for Loki system
 * 
 * Provides leveled logging (DEBUG, INFO, WARN, ERROR, CRITICAL)
 * with timestamp and source location information.
 * 
 * Usage:
 *   LOG_INFO("System initialized");
 *   LOG_ERROR("SPI initialization failed with code %d", error_code);
 *   LOG_DEBUG("Register value: 0x%02X", reg_value);
 */

#include <stdio.h>
#include <time.h>

/* ===== LOG LEVELS ===== */
typedef enum {
    LOG_CRITICAL = 0,  /* System failure, immediate action required */
    LOG_ERROR    = 1,  /* Error condition, operation failed */
    LOG_WARN     = 2,  /* Warning, unexpected but recoverable */
    LOG_INFO     = 3,  /* Informational messages */
    LOG_DEBUG    = 4,  /* Debug information, verbose */
} log_level_t;

/* ===== DYNAMIC LOG LEVEL CONTROL ===== */

/**
 * Set global log level (messages below this level are suppressed)
 * @param[in] level New log level
 */
void log_set_level(log_level_t level);

/**
 * Get current log level
 * @return Current log level
 */
log_level_t log_get_level(void);

/* ===== LOGGING MACROS ===== */

/**
 * @def LOG_CRITICAL
 * Log critical error with automatic source location
 */
#define LOG_CRITICAL(fmt, ...) \
    log_message(LOG_CRITICAL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @def LOG_ERROR
 * Log error with automatic source location
 */
#define LOG_ERROR(fmt, ...) \
    log_message(LOG_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @def LOG_WARN
 * Log warning with automatic source location
 */
#define LOG_WARN(fmt, ...) \
    log_message(LOG_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @def LOG_INFO
 * Log informational message
 */
#define LOG_INFO(fmt, ...) \
    log_message(LOG_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @def LOG_DEBUG
 * Log debug message (only in DEBUG builds)
 */
#ifdef DEBUG
#define LOG_DEBUG(fmt, ...) \
    log_message(LOG_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)  /* Debug logs disabled in release */
#endif

/* ===== CORE LOGGING FUNCTION ===== */

/**
 * Internal logging function (use macros instead)
 * @param[in] level Log level
 * @param[in] file Source file name
 * @param[in] line Source line number
 * @param[in] func Function name
 * @param[in] fmt Format string
 */
void log_message(log_level_t level, const char *file, int line, 
                 const char *func, const char *fmt, ...);

/**
 * Flush log output (if using file logging)
 */
void log_flush(void);

/**
 * Initialize logging system
 * @return 0 on success
 */
int log_init(void);

/**
 * Deinitialize logging system
 */
void log_deinit(void);

#endif /* LOG_H */

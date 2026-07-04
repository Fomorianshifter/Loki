/**
 * @file log.c
 * @brief Optimized logging framework implementation
 * 
 * Performance optimizations:
 * - Cached timestamps (updated per second, not per message)
 * - Buffered output (reduces system calls)
 * - Lazy file path extraction
 * - Early exit for disabled levels
 */

#include "log.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ===== LOG CONFIGURATION ===== */

#define LOG_BUFFER_SIZE 2048           /* Output buffer size */
#define TIMESTAMP_CACHE_INTERVAL_MS 1000  /* Update timestamp every 1 second */

/* ===== LOG STATE ===== */

static log_level_t global_log_level = LOG_INFO;
static char _log_buffer[LOG_BUFFER_SIZE] = {0};
static size_t _log_buffer_pos = 0;

/* Cached timestamp to avoid frequent system calls */
static char _cached_timestamp[16] = {0};
static time_t _cached_time = 0;

/* Map log levels to strings */
static const char *log_level_names[] = {
    "CRIT",  /* LOG_CRITICAL */
    "ERR ",  /* LOG_ERROR */
    "WARN",  /* LOG_WARN */
    "INFO",  /* LOG_INFO */
    "DBG ",  /* LOG_DEBUG */
};

/* ANSI color codes for terminal output */
static const char *log_colors[] = {
    "\033[1;31m",  /* CRITICAL - Bold Red */
    "\033[0;31m",  /* ERROR - Red */
    "\033[0;33m",  /* WARN - Yellow */
    "\033[0;32m",  /* INFO - Green */
    "\033[0;36m",  /* DEBUG - Cyan */
};
static const char *log_reset = "\033[0m";

/* ===== TIMESTAMP CACHING ===== */

static const char* _get_cached_timestamp(void)
{
    time_t now = time(NULL);
    
    /* Only update timestamp if 1+ second has passed */
    if (now != _cached_time) {
        _cached_time = now;
        struct tm *timeinfo = localtime(&now);
        if (timeinfo != NULL) {
            strftime(_cached_timestamp, sizeof(_cached_timestamp), "%H:%M:%S", timeinfo);
        }
    }
    
    return _cached_timestamp;
}

/* ===== LAZY FILE PATH EXTRACTION ===== */

static const char* _extract_filename(const char *file)
{
    if (file == NULL) {
        return "unknown";
    }
    
    /* Search for last forward slash (Unix paths) */
    const char *slash = strrchr(file, '/');
    if (slash != NULL) {
        return slash + 1;
    }
    
    /* Search for last backslash (Windows paths) */
    slash = strrchr(file, '\\');
    if (slash != NULL) {
        return slash + 1;
    }
    
    /* No path separators, return as-is */
    return file;
}

/* ===== BUFFERED OUTPUT ===== */

static void _flush_log_buffer(void)
{
    if (_log_buffer_pos > 0) {
        fwrite(_log_buffer, 1, _log_buffer_pos, stderr);
        _log_buffer_pos = 0;
    }
}

static void _write_to_buffer(const char *data, size_t len)
{
    if (len == 0) {
        return;
    }
    
    /* Flush if buffer would overflow */
    if (_log_buffer_pos + len > LOG_BUFFER_SIZE) {
        _flush_log_buffer();
    }
    
    /* Append to buffer */
    if (_log_buffer_pos + len <= LOG_BUFFER_SIZE) {
        memcpy(_log_buffer + _log_buffer_pos, data, len);
        _log_buffer_pos += len;
    }
}

/* ===== PUBLIC IMPLEMENTATION ===== */

void log_set_level(log_level_t level)
{
    global_log_level = level;
}

log_level_t log_get_level(void)
{
    return global_log_level;
}

void log_message(log_level_t level, const char *file, int line,
                 const char *func, const char *fmt, ...)
{
    /* OPTIMIZATION: Early exit for disabled levels */
    if (level > global_log_level) {
        return;
    }

    va_list args;
    va_start(args, fmt);

    /* OPTIMIZATION: Get cached timestamp (updates only per second) */
    const char *timestamp = _get_cached_timestamp();
    
    /* OPTIMIZATION: Lazy file path extraction */
    const char *filename = _extract_filename(file);

    /* Build and write header */
    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "%s[%s] %s%-5s%s [%s:%d] %s(): ",
                              log_colors[level], timestamp, log_colors[level],
                              log_level_names[level], log_reset,
                              filename, line, func);
    
    if (header_len > 0) {
        _write_to_buffer(header, (size_t)header_len);
    }

    /* Build and write message */
    char message[512];
    int msg_len = vsnprintf(message, sizeof(message), fmt, args);
    
    if (msg_len > 0) {
        _write_to_buffer(message, (size_t)msg_len);
    }

    /* Write newline */
    _write_to_buffer("\n", 1);
    
    /* Flush critical messages immediately; buffer others */
    if (level <= LOG_ERROR) {
        _flush_log_buffer();
    }

    va_end(args);
}

void log_flush(void)
{
    _flush_log_buffer();
    fflush(stderr);
    fflush(stdout);
}

int log_init(void)
{
    /* Initialize cached timestamp */
    _cached_time = time(NULL);
    struct tm *timeinfo = localtime(&_cached_time);
    if (timeinfo != NULL) {
        strftime(_cached_timestamp, sizeof(_cached_timestamp), "%H:%M:%S", timeinfo);
    }
    
    LOG_INFO("Logging system initialized (level: %s)", log_level_names[global_log_level]);
    return 0;
}

void log_deinit(void)
{
    LOG_INFO("Logging system shutting down");
    log_flush();
}

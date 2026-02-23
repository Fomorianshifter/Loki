/**
 * @file log.c
 * @brief Logging framework implementation
 */

#include "log.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

/* ===== LOG STATE ===== */
static log_level_t global_log_level = LOG_INFO;

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
    /* Skip messages below current log level */
    if (level > global_log_level) {
        return;
    }

    va_list args;
    va_start(args, fmt);

    /* Extract just the filename without full path */
    const char *filename = strrchr(file, '/');
    if (filename == NULL) {
        filename = strrchr(file, '\\');  /* Windows paths */
    }
    if (filename != NULL) {
        filename++;  /* Skip the slash */
    } else {
        filename = file;
    }

    /* Build timestamp */
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);

    /* Print header */
    fprintf(stderr, "%s[%s] %s%-5s%s [%s:%d] %s(): ",
            log_colors[level], timestamp, log_colors[level],
            log_level_names[level], log_reset,
            filename, line, func);

    /* Print message */
    vfprintf(stderr, fmt, args);

    /* Print newline */
    fprintf(stderr, "\n");
    fflush(stderr);

    va_end(args);
}

void log_flush(void)
{
    fflush(stderr);
    fflush(stdout);
}

int log_init(void)
{
    LOG_INFO("Logging system initialized (level: %s)", log_level_names[global_log_level]);
    return 0;
}

void log_deinit(void)
{
    LOG_INFO("Logging system shutting down");
    log_flush();
}

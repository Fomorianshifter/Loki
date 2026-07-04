#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H

/**
 * @file config_defaults.h
 * @brief Compile-time default values for all Loki subsystems.
 *
 * These values are used:
 *   1. Before the SD card config is loaded (early boot).
 *   2. As fallback when a key is missing from loki_config.ini.
 *   3. When safe-mode is active and SD config is bypassed.
 *
 * Do NOT put hardware pin/address constants here; those stay in pinout.h
 * and board_config.h.  These are *runtime behaviour* defaults.
 */

/* ===== SYSTEM ===== */
#define DEFAULT_STARTUP_MODE        "normal"   /* normal | safe | demo */
#define DEFAULT_LOG_LEVEL           "info"     /* debug | info | warn | error | critical */
#define DEFAULT_SAFE_MODE           0          /* 0=off, 1=on */

/* ===== AI ===== */
#define DEFAULT_AI_ENABLED          0          /* AI subsystem disabled by default */
#define DEFAULT_AI_MODEL            "none"
#define DEFAULT_AI_MAX_TOKENS       512

/* ===== PLUGINS ===== */
#define DEFAULT_PLUGIN_COUNT        0
#define MAX_PLUGINS                 16
#define MAX_PLUGIN_NAME_LEN         32

/* ===== PROGRAMS ===== */
#define DEFAULT_PROGRAM_COUNT       0
#define MAX_PROGRAMS                16
#define MAX_PROGRAM_NAME_LEN        32
#define MAX_PROGRAM_PATH_LEN        128

/* ===== STORAGE ===== */
#define DEFAULT_SD_CONFIG_ENABLED   1          /* Try SD card config by default */
#define DEFAULT_SD_MOUNT_POINT      "/loki"    /* Root path on SD card */
#define DEFAULT_CONFIG_PATH         "/loki/loki_config.ini"
#define DEFAULT_FALLBACK_TO_DEFAULTS 1

/* ===== DISPLAY ===== */
#define DEFAULT_BRIGHTNESS          80         /* Percent (0–100) */
#define DEFAULT_ROTATION            0          /* 0=Normal, 1=90°, 2=180°, 3=270° */

/* ===== FLIPPER ===== */
#define DEFAULT_FLIPPER_ENABLED     1
#define DEFAULT_FLIPPER_BAUD        115200

/* ===== MOOD ===== */
#define DEFAULT_MOOD_ENABLED        0
#define DEFAULT_MOOD                "neutral"

/* ===== SAFETY ===== */
#define DEFAULT_RECOVERY_ENABLED    1
#define DEFAULT_SAFE_MODE_ON_BOOT   0
#define DEFAULT_WATCHDOG_TIMEOUT_S  30

/* ===== CONFIG PARSER LIMITS ===== */
#define CONFIG_MAX_LINE_LEN         256
#define CONFIG_MAX_KEY_LEN          64
#define CONFIG_MAX_VAL_LEN          128
#define CONFIG_MAX_SECTION_LEN      32

#endif /* CONFIG_DEFAULTS_H */

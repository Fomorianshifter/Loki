#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

/**
 * @file config_manager.h
 * @brief Central runtime configuration manager for Loki.
 *
 * Boot sequence owned by this module:
 *   1. Load compile-time defaults (config_defaults.h)
 *   2. Initialize EEPROM and read bootstrap/failsafe metadata
 *   3. Initialize SD card
 *   4. Load master config from SD card  (loki_config.ini)
 *   5. Validate and apply configuration
 *   6. Initialize plugins and programs according to config
 *   7. Fall back safely if SD config is missing or corrupt
 *
 * The config manager is called from system_init() in system.c after the
 * low-level HAL and drivers are ready.
 *
 * Config source priority (highest wins):
 *   EEPROM safe_mode_flag=1  →  force safe mode, ignore SD config
 *   SD card loki_config.ini  →  primary runtime config
 *   compile-time defaults    →  always-available fallback
 */

#include "types.h"
#include "config_defaults.h"

/* ===== CONFIG SOURCE FLAGS ===== */
typedef enum {
    CFG_SOURCE_DEFAULTS = 0,   /**< Compile-time defaults only             */
    CFG_SOURCE_EEPROM   = 1,   /**< Overridden by EEPROM bootstrap flags   */
    CFG_SOURCE_SD       = 2,   /**< Full config loaded from SD card        */
} config_source_t;

/* ===== RUNTIME CONFIG STRUCT ===== */

/** AI subsystem config */
typedef struct {
    uint8_t  enabled;
    char     model[MAX_PLUGIN_NAME_LEN];
    uint16_t max_tokens;
} cfg_ai_t;

/** Single plugin config */
typedef struct {
    char    name[MAX_PLUGIN_NAME_LEN];
    uint8_t enabled;
    uint8_t order;
} cfg_plugin_t;

/** Single program config */
typedef struct {
    char    name[MAX_PROGRAM_NAME_LEN];
    char    path[MAX_PROGRAM_PATH_LEN];
    uint8_t enabled;
    uint8_t auto_start;
} cfg_program_t;

/** Storage config */
typedef struct {
    uint8_t sd_config_enabled;
    char    sd_mount_point[MAX_PROGRAM_PATH_LEN];
    uint8_t fallback_to_defaults;
} cfg_storage_t;

/** Display config */
typedef struct {
    uint8_t brightness;
    uint8_t rotation;
} cfg_display_t;

/** Flipper UART config */
typedef struct {
    uint8_t  enabled;
    uint32_t baud_rate;
} cfg_flipper_t;

/** Mood subsystem config */
typedef struct {
    uint8_t enabled;
    char    default_mood[MAX_PLUGIN_NAME_LEN];
} cfg_mood_t;

/** Safety / recovery config */
typedef struct {
    uint8_t  safe_mode_on_boot;
    uint8_t  recovery_enabled;
    uint16_t watchdog_timeout_s;
} cfg_safety_t;

/** Master runtime config (all sections combined) */
typedef struct {
    /* [system] */
    char    startup_mode[16];
    char    log_level[16];
    uint8_t safe_mode;

    /* subsection structs */
    cfg_ai_t       ai;
    cfg_plugin_t   plugins[MAX_PLUGINS];
    uint8_t        plugin_count;
    cfg_program_t  programs[MAX_PROGRAMS];
    uint8_t        program_count;
    cfg_storage_t  storage;
    cfg_display_t  display;
    cfg_flipper_t  flipper;
    cfg_mood_t     mood;
    cfg_safety_t   safety;

    /* metadata */
    config_source_t source;     /**< Where config was ultimately loaded from */
} loki_config_t;

/* ===== API ===== */

/**
 * Run the full config manager boot sequence.
 *
 * Calls the steps listed in the file header in order.  This is the only
 * function system_init() needs to call.
 *
 * Prerequisites: low-level HAL (GPIO, SPI, I2C) already initialized.
 *
 * @return HAL_OK on success (even if fallback to defaults was used).
 *         HAL_ERROR only for unrecoverable failures (e.g., no memory).
 */
hal_status_t config_manager_init(void);

/**
 * Return a pointer to the active runtime config.
 *
 * Valid after config_manager_init() returns HAL_OK.
 * The returned pointer remains valid for the lifetime of the process.
 *
 * @return Pointer to active loki_config_t, or NULL before init.
 */
const loki_config_t *config_manager_get(void);

/**
 * Return which source the config was loaded from.
 */
config_source_t config_manager_source(void);

/**
 * Reload the config from SD card and re-apply.
 *
 * Useful for a hot-reload command from the Flipper or UART console.
 *
 * @return HAL_OK on success
 */
hal_status_t config_manager_reload(void);

/**
 * Persist a small set of fields back to the EEPROM bootstrap block.
 *
 * Call this after changing runtime state that should survive a power cycle
 * (e.g., after incrementing boot count, setting safe mode, etc.).
 *
 * @return HAL_OK on success
 */
hal_status_t config_manager_save_bootstrap(void);

/**
 * Shut down the config manager (called from system_shutdown).
 */
void config_manager_deinit(void);

#endif /* CONFIG_MANAGER_H */

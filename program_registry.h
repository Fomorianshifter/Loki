#ifndef PROGRAM_REGISTRY_H
#define PROGRAM_REGISTRY_H

/**
 * @file program_registry.h
 * @brief Program registry — describes and manages external programs Loki can run.
 *
 * A "program" is an external binary or script that Loki can launch from the
 * SD card.  Examples: credits_vending, ai_assistant, game_launcher.
 *
 * Programs are defined in the [programs] section of loki_config.ini and
 * loaded by the config manager at startup.  Unlike plugins (which are
 * compiled-in subsystems), programs live on the SD card and are executed
 * as child processes or invoked via a platform-specific launcher.
 *
 * This registry provides the metadata layer; actual execution is handled
 * by the caller (e.g., main loop or a scheduler).
 */

#include "types.h"
#include "config_defaults.h"

/* ===== PROGRAM DESCRIPTOR ===== */

/**
 * Describes a single externally-runnable program.
 */
typedef struct {
    char    name[MAX_PROGRAM_NAME_LEN];  /**< Unique program identifier           */
    char    path[MAX_PROGRAM_PATH_LEN];  /**< Path on SD card or filesystem       */
    uint8_t enabled;                     /**< 1 = enabled, 0 = disabled           */
    uint8_t auto_start;                  /**< 1 = launch automatically at startup */
    uint8_t running;                     /**< 1 if currently executing            */
} program_entry_t;

/* ===== API ===== */

/**
 * Register a program with the registry.
 *
 * Typically called by the config manager while parsing [programs] section.
 *
 * @param[in] name        Unique program identifier
 * @param[in] path        File path on SD card
 * @param[in] enabled     1 = enabled
 * @param[in] auto_start  1 = launch at startup
 * @return HAL_OK on success, HAL_ERROR if registry is full or name duplicate
 */
hal_status_t program_registry_register(const char *name,
                                        const char *path,
                                        uint8_t enabled,
                                        uint8_t auto_start);

/**
 * Enable or disable a registered program.
 *
 * @param[in] name     Program name
 * @param[in] enabled  1 = enable, 0 = disable
 * @return HAL_OK on success, HAL_NOT_READY if not found
 */
hal_status_t program_registry_set_enabled(const char *name, uint8_t enabled);

/**
 * Look up a program entry by name.
 *
 * @param[in] name  Program name
 * @return Pointer to entry, or NULL if not found
 */
program_entry_t *program_registry_find(const char *name);

/**
 * Return total number of registered programs.
 */
uint8_t program_registry_count(void);

/**
 * Return pointer to the i-th registered program (for iteration).
 * Returns NULL when i >= program_registry_count().
 */
program_entry_t *program_registry_get(uint8_t index);

/**
 * Clear all registered programs (for testing / re-init).
 */
void program_registry_clear(void);

/**
 * Log a summary of all registered programs (INFO level).
 */
void program_registry_print_all(void);

#endif /* PROGRAM_REGISTRY_H */

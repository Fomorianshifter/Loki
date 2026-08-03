#ifndef PLUGIN_REGISTRY_H
#define PLUGIN_REGISTRY_H

/**
 * @file plugin_registry.h
 * @brief Plugin registry — describes and manages optional Loki subsystems.
 *
 * A "plugin" is any optional subsystem that can be enabled or disabled at
 * runtime through the master config (loki_config.ini).  Examples include
 * mood_engine, credits_display, flipper_bridge, etc.
 *
 * Plugins are registered at compile/link time via plugin_registry_register().
 * The config manager then enables or disables each entry and applies the
 * startup order defined in the config file.
 *
 * Lifecycle per plugin (when enabled):
 *   1. plugin_init_fn  — called once during system startup
 *   2. (running)
 *   3. plugin_deinit_fn — called during system shutdown
 */

#include "types.h"
#include "config_defaults.h"

/* ===== PLUGIN DESCRIPTOR ===== */

/** Function pointer types for plugin lifecycle hooks */
typedef hal_status_t (*plugin_init_fn)(void);
typedef hal_status_t (*plugin_deinit_fn)(void);

/**
 * Describes a single plugin.
 * Populated by plugin_registry_register() and updated by the config manager.
 */
typedef struct {
    char             name[MAX_PLUGIN_NAME_LEN]; /**< Unique plugin identifier      */
    uint8_t          enabled;                   /**< 1 = enabled, 0 = disabled     */
    uint8_t          order;                     /**< Startup order (lower = first) */
    plugin_init_fn   init;                      /**< Init hook (may be NULL)       */
    plugin_deinit_fn deinit;                    /**< Deinit hook (may be NULL)     */
    uint8_t          initialized;               /**< 1 after successful init       */
} plugin_entry_t;

/* ===== API ===== */

/**
 * Register a plugin with the registry.
 *
 * Must be called before config_manager_apply() so the config manager can
 * set the enabled flag and startup order from the config file.
 *
 * @param[in] name    Unique plugin name (must match key in [plugins] section)
 * @param[in] init    Init function pointer (NULL = no init needed)
 * @param[in] deinit  Deinit function pointer (NULL = no deinit needed)
 * @return HAL_OK on success, HAL_ERROR if registry is full
 */
hal_status_t plugin_registry_register(const char *name,
                                       plugin_init_fn init,
                                       plugin_deinit_fn deinit);

/**
 * Enable or disable a registered plugin.
 *
 * @param[in] name     Plugin name
 * @param[in] enabled  1 = enable, 0 = disable
 * @return HAL_OK on success, HAL_NOT_READY if plugin not found
 */
hal_status_t plugin_registry_set_enabled(const char *name, uint8_t enabled);

/**
 * Set startup order for a registered plugin.
 *
 * @param[in] name   Plugin name
 * @param[in] order  Startup order value (lower = earlier)
 * @return HAL_OK on success
 */
hal_status_t plugin_registry_set_order(const char *name, uint8_t order);

/**
 * Initialize all enabled plugins in startup-order sequence.
 * Called by the config manager after applying configuration.
 *
 * @return HAL_OK if all enabled plugins initialized, HAL_ERROR on first failure
 */
hal_status_t plugin_registry_init_all(void);

/**
 * Deinitialize all initialized plugins in reverse startup-order sequence.
 * Called during system shutdown.
 *
 * @return HAL_OK on success
 */
hal_status_t plugin_registry_deinit_all(void);

/**
 * Look up a plugin entry by name.
 *
 * @param[in] name  Plugin name
 * @return Pointer to entry, or NULL if not found
 */
plugin_entry_t *plugin_registry_find(const char *name);

/**
 * Return total number of registered plugins.
 */
uint8_t plugin_registry_count(void);

/**
 * Return pointer to the i-th registered plugin (for iteration).
 * Returns NULL when i >= plugin_registry_count().
 */
plugin_entry_t *plugin_registry_get(uint8_t index);

/**
 * Clear all registered plugins (for testing / re-init).
 */
void plugin_registry_clear(void);

#endif /* PLUGIN_REGISTRY_H */

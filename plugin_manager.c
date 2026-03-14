/**
 * @file plugin_manager.c
 * @brief Plugin registry implementation.
 */

#include "plugin_manager.h"

#include <string.h>

#include "log.h"

void loki_plugin_manager_init(loki_plugin_manager_t *manager)
{
    if (manager == NULL) {
        return;
    }

    memset(manager, 0, sizeof(*manager));
}

hal_status_t loki_plugin_manager_register(loki_plugin_manager_t *manager, const loki_plugin_t *plugin)
{
    if (manager == NULL || plugin == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (manager->count >= LOKI_MAX_PLUGINS) {
        LOG_ERROR("Plugin registry full, cannot register %s", plugin->name);
        return HAL_BUSY;
    }

    manager->plugins[manager->count] = *plugin;
    manager->count++;
    LOG_INFO("Registered plugin: %s (%s)", plugin->name, plugin->enabled ? "enabled" : "disabled");
    return HAL_OK;
}

hal_status_t loki_plugin_manager_start(loki_plugin_manager_t *manager)
{
    size_t index;

    if (manager == NULL) {
        return HAL_INVALID_PARAM;
    }

    for (index = 0; index < manager->count; index++) {
        loki_plugin_t *plugin = &manager->plugins[index];

        if (!plugin->enabled || plugin->init == NULL) {
            continue;
        }

        if (plugin->init(plugin) == HAL_OK) {
            plugin->initialized = true;
            LOG_INFO("Plugin started: %s", plugin->name);
        } else {
            plugin->enabled = false;
            LOG_WARN("Plugin disabled after init failure: %s", plugin->name);
        }
    }

    return HAL_OK;
}

hal_status_t loki_plugin_manager_update(loki_plugin_manager_t *manager, uint32_t tick_ms)
{
    size_t index;

    if (manager == NULL) {
        return HAL_INVALID_PARAM;
    }

    for (index = 0; index < manager->count; index++) {
        loki_plugin_t *plugin = &manager->plugins[index];

        if (!plugin->enabled || !plugin->initialized || plugin->update == NULL) {
            continue;
        }

        if (plugin->update(plugin, tick_ms) != HAL_OK) {
            LOG_WARN("Plugin update reported a recoverable issue: %s", plugin->name);
        }
    }

    return HAL_OK;
}

hal_status_t loki_plugin_manager_stop(loki_plugin_manager_t *manager)
{
    size_t index;

    if (manager == NULL) {
        return HAL_INVALID_PARAM;
    }

    for (index = manager->count; index > 0; index--) {
        loki_plugin_t *plugin = &manager->plugins[index - 1];

        if (!plugin->initialized || plugin->shutdown == NULL) {
            continue;
        }

        plugin->shutdown(plugin);
        plugin->initialized = false;
        LOG_INFO("Plugin stopped: %s", plugin->name);
    }

    return HAL_OK;
}
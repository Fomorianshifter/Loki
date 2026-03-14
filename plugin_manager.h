#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

/**
 * @file plugin_manager.h
 * @brief Lightweight plugin registry for Loki runtime modules.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "types.h"

#define LOKI_MAX_PLUGINS  8

typedef struct loki_plugin loki_plugin_t;

typedef hal_status_t (*loki_plugin_init_fn)(loki_plugin_t *plugin);
typedef hal_status_t (*loki_plugin_update_fn)(loki_plugin_t *plugin, uint32_t tick_ms);
typedef hal_status_t (*loki_plugin_shutdown_fn)(loki_plugin_t *plugin);

struct loki_plugin {
    const char *name;
    bool enabled;
    bool initialized;
    void *context;
    loki_plugin_init_fn init;
    loki_plugin_update_fn update;
    loki_plugin_shutdown_fn shutdown;
};

typedef struct {
    loki_plugin_t plugins[LOKI_MAX_PLUGINS];
    size_t count;
} loki_plugin_manager_t;

void loki_plugin_manager_init(loki_plugin_manager_t *manager);
hal_status_t loki_plugin_manager_register(loki_plugin_manager_t *manager, const loki_plugin_t *plugin);
hal_status_t loki_plugin_manager_start(loki_plugin_manager_t *manager);
hal_status_t loki_plugin_manager_update(loki_plugin_manager_t *manager, uint32_t tick_ms);
hal_status_t loki_plugin_manager_stop(loki_plugin_manager_t *manager);

#endif /* PLUGIN_MANAGER_H */
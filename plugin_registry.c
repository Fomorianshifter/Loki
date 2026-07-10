/**
 * @file plugin_registry.c
 * @brief Plugin registry implementation.
 */

#include "plugin_registry.h"
#include "log.h"
#include <string.h>

/* ===== REGISTRY STORAGE ===== */

static plugin_entry_t registry[MAX_PLUGINS];
static uint8_t        registry_count = 0;

/* ===== HELPERS ===== */

static plugin_entry_t *find_by_name(const char *name)
{
    for (uint8_t i = 0; i < registry_count; i++) {
        if (strncmp(registry[i].name, name, MAX_PLUGIN_NAME_LEN) == 0) {
            return &registry[i];
        }
    }
    return NULL;
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t plugin_registry_register(const char *name,
                                       plugin_init_fn init,
                                       plugin_deinit_fn deinit)
{
    if (name == NULL || name[0] == '\0') {
        return HAL_INVALID_PARAM;
    }

    if (registry_count >= MAX_PLUGINS) {
        LOG_ERROR("Plugin registry full (MAX_PLUGINS=%d)", MAX_PLUGINS);
        return HAL_ERROR;
    }

    if (find_by_name(name) != NULL) {
        LOG_WARN("Plugin '%s' already registered", name);
        return HAL_ERROR;
    }

    plugin_entry_t *entry = &registry[registry_count++];
    memset(entry, 0, sizeof(plugin_entry_t));
    strncpy(entry->name, name, MAX_PLUGIN_NAME_LEN - 1);
    entry->init        = init;
    entry->deinit      = deinit;
    entry->enabled     = 0;   /* disabled until config applies */
    entry->order       = registry_count;  /* default order = registration order */
    entry->initialized = 0;

    LOG_DEBUG("Registered plugin '%s'", name);
    return HAL_OK;
}

hal_status_t plugin_registry_set_enabled(const char *name, uint8_t enabled)
{
    plugin_entry_t *entry = find_by_name(name);
    if (entry == NULL) {
        LOG_WARN("plugin_registry_set_enabled: unknown plugin '%s'", name);
        return HAL_NOT_READY;
    }
    entry->enabled = enabled ? 1 : 0;
    return HAL_OK;
}

hal_status_t plugin_registry_set_order(const char *name, uint8_t order)
{
    plugin_entry_t *entry = find_by_name(name);
    if (entry == NULL) {
        LOG_WARN("plugin_registry_set_order: unknown plugin '%s'", name);
        return HAL_NOT_READY;
    }
    entry->order = order;
    return HAL_OK;
}

/* Sort an indices[] array by registry[].order (insertion sort, ascending). */
static void sort_by_order(uint8_t *indices, uint8_t count)
{
    /* Simple insertion sort — count is small (≤ MAX_PLUGINS = 16).
     * Each element of indices[] is a registry array subscript.
     * 'cur_idx' stores the registry subscript from indices[i] so we can
     * compare registry[cur_idx].order against registry[indices[j]].order. */
    for (uint8_t i = 1; i < count; i++) {
        uint8_t cur_idx = indices[i]; /* registry subscript being inserted */
        int8_t  j       = (int8_t)(i - 1);
        while (j >= 0 && registry[indices[j]].order > registry[cur_idx].order) {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = cur_idx;
    }
}

hal_status_t plugin_registry_init_all(void)
{
    /* Build a sorted index of enabled plugins */
    uint8_t enabled_idx[MAX_PLUGINS];
    uint8_t enabled_count = 0;

    for (uint8_t i = 0; i < registry_count; i++) {
        if (registry[i].enabled) {
            enabled_idx[enabled_count++] = i;
        }
    }
    sort_by_order(enabled_idx, enabled_count);

    LOG_INFO("Initializing %d enabled plugin(s)...", enabled_count);

    for (uint8_t i = 0; i < enabled_count; i++) {
        plugin_entry_t *p = &registry[enabled_idx[i]];
        LOG_INFO("  [plugin] '%s' (order=%d)", p->name, p->order);

        if (p->init != NULL) {
            hal_status_t st = p->init();
            if (st != HAL_OK) {
                LOG_ERROR("  Plugin '%s' init failed (status=%d)", p->name, st);
                return HAL_ERROR;
            }
        }
        p->initialized = 1;
    }

    return HAL_OK;
}

hal_status_t plugin_registry_deinit_all(void)
{
    /* Deinit in reverse startup order */
    uint8_t init_idx[MAX_PLUGINS];
    uint8_t init_count = 0;

    for (uint8_t i = 0; i < registry_count; i++) {
        if (registry[i].initialized) {
            init_idx[init_count++] = i;
        }
    }
    sort_by_order(init_idx, init_count);

    /* Reverse iterate */
    for (int8_t i = (int8_t)(init_count - 1); i >= 0; i--) {
        plugin_entry_t *p = &registry[init_idx[i]];
        if (p->deinit != NULL) {
            p->deinit();
        }
        p->initialized = 0;
    }
    return HAL_OK;
}

plugin_entry_t *plugin_registry_find(const char *name)
{
    return find_by_name(name);
}

uint8_t plugin_registry_count(void)
{
    return registry_count;
}

plugin_entry_t *plugin_registry_get(uint8_t index)
{
    if (index >= registry_count) {
        return NULL;
    }
    return &registry[index];
}

void plugin_registry_clear(void)
{
    memset(registry, 0, sizeof(registry));
    registry_count = 0;
}

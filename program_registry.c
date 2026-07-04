/**
 * @file program_registry.c
 * @brief Program registry implementation.
 */

#include "program_registry.h"
#include "log.h"
#include <string.h>

/* ===== REGISTRY STORAGE ===== */

static program_entry_t registry[MAX_PROGRAMS];
static uint8_t         registry_count = 0;

/* ===== HELPERS ===== */

static program_entry_t *find_by_name(const char *name)
{
    for (uint8_t i = 0; i < registry_count; i++) {
        if (strncmp(registry[i].name, name, MAX_PROGRAM_NAME_LEN) == 0) {
            return &registry[i];
        }
    }
    return NULL;
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t program_registry_register(const char *name,
                                        const char *path,
                                        uint8_t     enabled,
                                        uint8_t     auto_start)
{
    if (name == NULL || name[0] == '\0') {
        return HAL_INVALID_PARAM;
    }
    if (path == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (registry_count >= MAX_PROGRAMS) {
        LOG_ERROR("Program registry full (MAX_PROGRAMS=%d)", MAX_PROGRAMS);
        return HAL_ERROR;
    }

    if (find_by_name(name) != NULL) {
        LOG_WARN("Program '%s' already registered", name);
        return HAL_ERROR;
    }

    program_entry_t *entry = &registry[registry_count++];
    memset(entry, 0, sizeof(program_entry_t));
    strncpy(entry->name, name, MAX_PROGRAM_NAME_LEN - 1);
    strncpy(entry->path, path, MAX_PROGRAM_PATH_LEN - 1);
    entry->enabled    = enabled ? 1 : 0;
    entry->auto_start = auto_start ? 1 : 0;
    entry->running    = 0;

    LOG_DEBUG("Registered program '%s' path='%s' enabled=%d auto_start=%d",
              name, path, enabled, auto_start);
    return HAL_OK;
}

hal_status_t program_registry_set_enabled(const char *name, uint8_t enabled)
{
    program_entry_t *entry = find_by_name(name);
    if (entry == NULL) {
        LOG_WARN("program_registry_set_enabled: unknown program '%s'", name);
        return HAL_NOT_READY;
    }
    entry->enabled = enabled ? 1 : 0;
    return HAL_OK;
}

program_entry_t *program_registry_find(const char *name)
{
    return find_by_name(name);
}

uint8_t program_registry_count(void)
{
    return registry_count;
}

program_entry_t *program_registry_get(uint8_t index)
{
    if (index >= registry_count) {
        return NULL;
    }
    return &registry[index];
}

void program_registry_clear(void)
{
    memset(registry, 0, sizeof(registry));
    registry_count = 0;
}

void program_registry_print_all(void)
{
    LOG_INFO("Program registry (%d entries):", registry_count);
    for (uint8_t i = 0; i < registry_count; i++) {
        const program_entry_t *p = &registry[i];
        LOG_INFO("  [%d] '%s'  path='%s'  enabled=%d  auto_start=%d",
                 i, p->name, p->path, p->enabled, p->auto_start);
    }
}

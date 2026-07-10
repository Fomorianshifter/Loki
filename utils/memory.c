/**
 * @file memory.c
 * @brief Safe memory management implementation
 */

#include "memory.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

/* ===== MEMORY TRACKING (DEBUG MODE) ===== */
#ifdef DEBUG

typedef struct {
    void *ptr;
    size_t size;
    const char *location;
} allocation_t;

#define MAX_ALLOCATIONS 1024
static allocation_t alloc_table[MAX_ALLOCATIONS] = {0};
static size_t alloc_count = 0;
static size_t total_allocated = 0;

static int find_allocation(void *ptr)
{
    for (size_t i = 0; i < alloc_count; i++) {
        if (alloc_table[i].ptr == ptr) {
            return i;
        }
    }
    return -1;
}

static void track_allocation(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return;
    }

    if (alloc_count >= MAX_ALLOCATIONS) {
        LOG_WARN("Allocation table full, memory tracking disabled for new allocations");
        return;
    }

    alloc_table[alloc_count].ptr = ptr;
    alloc_table[alloc_count].size = size;
    alloc_table[alloc_count].location = "unknown";
    alloc_count++;
    total_allocated += size;

    LOG_DEBUG("Allocated %zu bytes at %p (total: %zu bytes)", size, ptr, total_allocated);
}

static void untrack_allocation(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    int idx = find_allocation(ptr);
    if (idx >= 0) {
        total_allocated -= alloc_table[idx].size;
        LOG_DEBUG("Freed %zu bytes from %p (total: %zu bytes)", 
                 alloc_table[idx].size, ptr, total_allocated);

        /* Remove from table */
        for (size_t i = idx; i < alloc_count - 1; i++) {
            alloc_table[i] = alloc_table[i + 1];
        }
        alloc_count--;
    }
}

#endif /* DEBUG */

/* ===== PUBLIC IMPLEMENTATION ===== */

void* malloc_safe(size_t size)
{
    if (size == 0) {
        LOG_WARN("Attempted to allocate 0 bytes");
        return NULL;
    }

    void *ptr = malloc(size);
    if (ptr == NULL) {
        LOG_ERROR("Memory allocation failed for %zu bytes", size);
        return NULL;
    }

#ifdef DEBUG
    track_allocation(ptr, size);
#endif

    return ptr;
}

void* calloc_safe(size_t count, size_t elem_size)
{
    if (count == 0 || elem_size == 0) {
        LOG_WARN("Attempted to allocate 0 elements or element size");
        return NULL;
    }

    void *ptr = calloc(count, elem_size);
    if (ptr == NULL) {
        LOG_ERROR("Memory allocation failed for %zu elements of %zu bytes each", count, elem_size);
        return NULL;
    }

#ifdef DEBUG
    track_allocation(ptr, count * elem_size);
#endif

    return ptr;
}

void free_safe(void **ptr)
{
    if (ptr == NULL || *ptr == NULL) {
        return;
    }

#ifdef DEBUG
    untrack_allocation(*ptr);
#endif

    free(*ptr);
    *ptr = NULL;
}

size_t memory_get_usage(void)
{
#ifdef DEBUG
    return total_allocated;
#else
    return 0;  /* Not tracking in release mode */
#endif
}

void memory_report(void)
{
#ifdef DEBUG
    LOG_INFO("=== Memory Allocation Report ===");
    LOG_INFO("Total allocated: %zu bytes", total_allocated);
    LOG_INFO("Active allocations: %zu", alloc_count);

    if (alloc_count > 0) {
        LOG_INFO("Allocations:");
        for (size_t i = 0; i < alloc_count; i++) {
            LOG_INFO("  [%zu] %zu bytes at %p", 
                    i, alloc_table[i].size, alloc_table[i].ptr);
        }
    }
    LOG_INFO("================================");
#else
    LOG_WARN("Memory tracking disabled in Release mode");
#endif
}

void memory_init(void)
{
#ifdef DEBUG
    memset(alloc_table, 0, sizeof(alloc_table));
    alloc_count = 0;
    total_allocated = 0;
    LOG_DEBUG("Memory tracking system initialized");
#endif
}

void memory_deinit(void)
{
#ifdef DEBUG
    if (alloc_count > 0) {
        LOG_WARN("Memory deinit: %zu allocations still active", alloc_count);
        memory_report();
    }
    memset(alloc_table, 0, sizeof(alloc_table));
    alloc_count = 0;
    total_allocated = 0;
#endif
}

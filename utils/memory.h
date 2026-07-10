#ifndef MEMORY_H
#define MEMORY_H

/**
 * @file memory.h
 * @brief Safe memory management utilities
 * 
 * Provides wrappers around malloc/free with error checking
 * and optional memory leak detection (DEBUG builds).
 */

#include <stddef.h>

/* ===== SAFE ALLOCATION ===== */

/**
 * Safe malloc with error checking
 * @param[in] size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 * 
 * Usage:
 *   uint8_t *buffer = malloc_safe(256);
 *   if (buffer == NULL) { LOG_ERROR("Out of memory"); return; }
 */
void* malloc_safe(size_t size);

/**
 * Safe calloc (allocates and zeros memory)
 * @param[in] count Number of elements
 * @param[in] elem_size Size of each element
 * @return Pointer to allocated zero-initialized memory, or NULL on failure
 */
void* calloc_safe(size_t count, size_t elem_size);

/**
 * Safe free with NULL check
 * @param[in,out] ptr Pointer to free (set to NULL after freeing)
 * 
 * Usage:
 *   free_safe(&buffer);  // buffer is now NULL
 */
void free_safe(void **ptr);

/**
 * Get memory allocation statistics (DEBUG only)
 * @return Total bytes currently allocated
 */
size_t memory_get_usage(void);

/**
 * Print memory allocation report (DEBUG only)
 */
void memory_report(void);

/**
 * Initialize memory tracking system
 */
void memory_init(void);

/**
 * Deinitialize memory tracking
 */
void memory_deinit(void);

#endif /* MEMORY_H */

#ifndef SYSTEM_H
#define SYSTEM_H

/**
 * System Initialization and Management
 * Orange Pi Zero 2W Loki Board
 */

#include "../includes/types.h"

/* ===== SYSTEM LIFECYCLE ===== */

/**
 * Initialize entire system (HAL + drivers)
 * @return HAL_OK on success
 */
hal_status_t system_init(void);

/**
 * Display system status (for debugging)
 */
void system_print_status(void);

/**
 * Shutdown system gracefully
 * @return HAL_OK on success
 */
hal_status_t system_shutdown(void);

#endif /* SYSTEM_H */

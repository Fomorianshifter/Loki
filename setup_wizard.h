#ifndef SETUP_WIZARD_H
#define SETUP_WIZARD_H

/**
 * @file setup_wizard.h
 * @brief First-boot onboarding scaffold for Loki.
 */

#include "runtime_config.h"

hal_status_t loki_setup_wizard_run(const char *config_path, loki_runtime_config_t *config);

#endif /* SETUP_WIZARD_H */

#include "setup_wizard.h"

#include "board_profile.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void prompt_text(const char *prompt, char *target, size_t target_size)
{
    char line[128];

    if (target == NULL || target_size == 0) {
        return;
    }

    fprintf(stdout, "%s", prompt);
    fflush(stdout);

    if (fgets(line, sizeof(line), stdin) != NULL) {
        char *nl = strchr(line, '\n');
        if (nl != NULL) {
            *nl = '\0';
        }

        if (line[0] != '\0') {
            strncpy(target, line, target_size - 1);
            target[target_size - 1] = '\0';
        }
    }
}

hal_status_t loki_setup_wizard_run(const char *config_path, loki_runtime_config_t *config)
{
    const loki_board_profile_t *default_profile = loki_board_profile_default();

    if (config_path == NULL || config == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (!config->device.first_boot) {
        return HAL_OK;
    }

    LOG_INFO("First boot detected. Starting setup wizard scaffold...");

    if (isatty(STDIN_FILENO)) {
        fprintf(stdout, "\n=== Loki First Boot Setup ===\n");
        fprintf(stdout, "Press Enter to keep defaults.\n\n");

        prompt_text("Device name [Loki]: ", config->device.name, sizeof(config->device.name));
        prompt_text("Theme [default]: ", config->device.theme, sizeof(config->device.theme));
        prompt_text("Board profile [raspberry_pi_zero_w/orange_pi_zero_2w/generic_linux_sbc]: ",
                    config->device.board_profile, sizeof(config->device.board_profile));

        if (!loki_board_profile_is_supported(config->device.board_profile)) {
            strncpy(config->device.board_profile, default_profile->id, sizeof(config->device.board_profile) - 1);
            config->device.board_profile[sizeof(config->device.board_profile) - 1] = '\0';
        }
    } else {
        LOG_INFO("Non-interactive boot; using defaults for setup scaffold");
    }

    config->device.first_boot = 0;

    if (loki_config_validate(config) != HAL_OK) {
        LOG_ERROR("Setup wizard validation failed");
        return HAL_ERROR;
    }

    if (loki_config_save(config_path, config) != HAL_OK) {
        LOG_ERROR("Setup wizard failed to save runtime config");
        return HAL_ERROR;
    }

    LOG_INFO("Setup wizard completed and config saved: %s", config_path);
    return HAL_OK;
}

#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

/**
 * @file runtime_config.h
 * @brief Master runtime config for Loki user/deployment settings.
 */

#include <stdint.h>

#include "types.h"

#define LOKI_CONFIG_PATH_DEFAULT "/etc/loki/loki.conf"
#define LOKI_CONFIG_PATH_FALLBACK "./loki.conf"

typedef struct {
    struct {
        char name[32];
        char species[32];
        char stage[16];
        char theme[32];
        char board_profile[48];
        uint8_t first_boot;
    } device;

    struct {
        uint8_t mood_enabled;
        char base_mood[16];
        uint8_t mood_decay_rate;
        uint8_t friendliness;
        uint8_t mischief;
        uint8_t energy;
    } personality;

    struct {
        uint8_t brightness;
        uint8_t rotation;
        uint8_t show_stats;
        uint8_t show_face;
        char idle_animation[32];
    } ui;

    struct {
        uint8_t auto_evolve;
        char evolution_speed[16];
        uint8_t sleep_enabled;
        uint8_t sound_enabled;
        uint16_t interaction_timeout_sec;
    } behavior;

    struct {
        uint8_t credits_enabled;
        uint32_t starting_balance;
        uint8_t allow_write;
        uint8_t allow_spend;
    } credits;

    struct {
        char level[16];
        uint8_t error_logging;
    } logging;

    struct {
        uint32_t spi_freq_tft;
        uint32_t spi_freq_flash;
        uint32_t i2c_freq;
        uint32_t uart_baud;
    } hardware_overrides;

    struct {
        char data_dir[128];
        char log_dir[128];
    } paths;
} loki_runtime_config_t;

void loki_config_set_defaults(loki_runtime_config_t *config);
hal_status_t loki_config_validate(loki_runtime_config_t *config);
hal_status_t loki_config_load(const char *path, loki_runtime_config_t *config);
hal_status_t loki_config_save(const char *path, const loki_runtime_config_t *config);
hal_status_t loki_config_load_or_create(const char *path, loki_runtime_config_t *config, uint8_t *created_default);

#endif /* RUNTIME_CONFIG_H */

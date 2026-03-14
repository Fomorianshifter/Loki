#ifndef LOKI_CONFIG_H
#define LOKI_CONFIG_H

/**
 * @file loki_config.h
 * @brief Centralized runtime configuration for Loki.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "types.h"

#define LOKI_CONFIG_PATH        "loki.conf"
#define LOKI_CONFIG_STRING_MAX  64
#define LOKI_TOOL_COMMAND_MAX   128
#define LOKI_TOOL_DESCRIPTION_MAX 96

typedef struct {
    bool display;
    bool dragon_ai;
    bool network_state;
} loki_plugin_config_t;

typedef struct {
    bool test_tft;
    bool test_flash;
    bool test_eeprom;
    bool test_flipper;
} loki_hardware_config_t;

typedef struct {
    char backend[LOKI_CONFIG_STRING_MAX];
    char framebuffer_device[LOKI_CONFIG_STRING_MAX];
} loki_display_config_t;

typedef struct {
    float learning_rate;
    float actor_learning_rate;
    float critic_learning_rate;
    float discount_factor;
    float reward_decay;
    float entropy_beta;
    float policy_temperature;
    uint32_t tick_interval_ms;
} loki_ai_config_t;

typedef struct {
    char profile[LOKI_CONFIG_STRING_MAX];
    char temperament[LOKI_CONFIG_STRING_MAX];
    float base_energy;
    float base_curiosity;
    float base_bond;
    float base_hunger;
    float hunger_rate;
    float energy_decay;
    float curiosity_rate;
    float mood_hungry_threshold;
    float mood_restful_energy_threshold;
    float mood_curious_threshold;
    float mood_excited_threshold;
    float rest_bias;
    float observe_bias;
    float explore_bias;
    float play_bias;
    float rest_reward;
    float observe_reward;
    float explore_reward;
    float play_reward;
    float rest_energy_gain;
    float rest_hunger_gain;
    float observe_bond_gain;
    float explore_curiosity_cost;
    float explore_energy_cost;
    float explore_xp_gain;
    float play_bond_gain;
    float play_energy_cost;
    float play_hunger_gain;
    float play_xp_gain;
    float hunger_penalty_threshold;
    float hunger_penalty;
    uint32_t growth_interval_ms;
    float growth_xp_step;
    uint32_t egg_stage_max;
    uint32_t hatchling_stage_max;
} loki_dragon_config_t;

typedef struct {
    char bind_address[LOKI_CONFIG_STRING_MAX];
    uint16_t port;
} loki_web_config_t;

typedef struct {
    char section[LOKI_CONFIG_STRING_MAX];
    bool enabled;
    char name[LOKI_CONFIG_STRING_MAX];
    char description[LOKI_TOOL_DESCRIPTION_MAX];
    char command[LOKI_TOOL_COMMAND_MAX];
} loki_tool_config_t;

typedef struct {
    char device_name[LOKI_CONFIG_STRING_MAX];
    char dragon_name[LOKI_CONFIG_STRING_MAX];
    char hostname[LOKI_CONFIG_STRING_MAX];
    char preferred_ssid[LOKI_CONFIG_STRING_MAX];
    bool dhcp_enabled;
    bool standalone_mode;
    bool announce_new_networks;
    bool web_ui_enabled;
    uint32_t scan_interval_ms;
    uint32_t hop_interval_ms;
    uint32_t discovery_hold_ms;
    loki_plugin_config_t plugins;
    loki_hardware_config_t hardware;
    loki_display_config_t display;
    loki_web_config_t web;
    loki_tool_config_t *tools;
    size_t tool_count;
    size_t tool_capacity;
    loki_ai_config_t ai;
    loki_dragon_config_t dragon;
} loki_config_t;

void loki_config_set_defaults(loki_config_t *config);
void loki_config_cleanup(loki_config_t *config);
hal_status_t loki_config_clone(loki_config_t *dest, const loki_config_t *src);
hal_status_t loki_config_ensure_tool(loki_config_t *config,
                                     const char *section,
                                     loki_tool_config_t **tool);
hal_status_t loki_config_load(const char *path, loki_config_t *config);
hal_status_t loki_config_save(const char *path, const loki_config_t *config);

#endif /* LOKI_CONFIG_H */
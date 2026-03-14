/**
 * @file loki_config.c
 * @brief Runtime configuration loader and saver.
 */

#include "loki_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dragon_ai.h"
#include "log.h"

static char *trim_whitespace(char *text)
{
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return text;
}

static bool parse_bool(const char *value)
{
    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0;
}

static void copy_string(char *dest, size_t dest_size, const char *src)
{
    size_t copy_length;

    if (dest == NULL || dest_size == 0U) {
        return;
    }

    if (src == NULL) {
        dest[0] = '\0';
        return;
    }

    copy_length = strlen(src);
    if (copy_length >= dest_size) {
        copy_length = dest_size - 1U;
    }

    memcpy(dest, src, copy_length);
    dest[copy_length] = '\0';
}

static void loki_tool_set_defaults(loki_tool_config_t *tool, const char *section)
{
    if (tool == NULL) {
        return;
    }

    memset(tool, 0, sizeof(*tool));
    copy_string(tool->section, sizeof(tool->section), section);
    copy_string(tool->name, sizeof(tool->name), "Unused");
    copy_string(tool->description, sizeof(tool->description), "Reserved for future safe integrations.");
}

static hal_status_t loki_config_reserve_tools(loki_config_t *config, size_t needed_capacity)
{
    loki_tool_config_t *new_tools;
    size_t new_capacity;

    if (config == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (needed_capacity <= config->tool_capacity) {
        return HAL_OK;
    }

    new_capacity = (config->tool_capacity == 0U) ? 4U : config->tool_capacity;
    while (new_capacity < needed_capacity) {
        new_capacity *= 2U;
    }

    new_tools = (loki_tool_config_t *)realloc(config->tools, new_capacity * sizeof(loki_tool_config_t));
    if (new_tools == NULL) {
        return HAL_ERROR;
    }

    memset(new_tools + config->tool_capacity,
           0,
           (new_capacity - config->tool_capacity) * sizeof(loki_tool_config_t));

    config->tools = new_tools;
    config->tool_capacity = new_capacity;
    return HAL_OK;
}

void loki_config_cleanup(loki_config_t *config)
{
    if (config == NULL) {
        return;
    }

    free(config->tools);
    config->tools = NULL;
    config->tool_count = 0U;
    config->tool_capacity = 0U;
}

hal_status_t loki_config_ensure_tool(loki_config_t *config,
                                     const char *section,
                                     loki_tool_config_t **tool)
{
    size_t index;

    if (config == NULL || section == NULL || section[0] == '\0') {
        return HAL_INVALID_PARAM;
    }

    for (index = 0; index < config->tool_count; index++) {
        if (strcmp(config->tools[index].section, section) == 0) {
            if (tool != NULL) {
                *tool = &config->tools[index];
            }
            return HAL_OK;
        }
    }

    if (loki_config_reserve_tools(config, config->tool_count + 1U) != HAL_OK) {
        return HAL_ERROR;
    }

    loki_tool_set_defaults(&config->tools[config->tool_count], section);
    if (tool != NULL) {
        *tool = &config->tools[config->tool_count];
    }
    config->tool_count++;
    return HAL_OK;
}

hal_status_t loki_config_clone(loki_config_t *dest, const loki_config_t *src)
{
    size_t index;

    if (dest == NULL || src == NULL) {
        return HAL_INVALID_PARAM;
    }

    memset(dest, 0, sizeof(*dest));
    *dest = *src;
    dest->tools = NULL;
    dest->tool_count = 0U;
    dest->tool_capacity = 0U;

    if (src->tool_count == 0U) {
        return HAL_OK;
    }

    if (loki_config_reserve_tools(dest, src->tool_count) != HAL_OK) {
        loki_config_cleanup(dest);
        return HAL_ERROR;
    }

    for (index = 0; index < src->tool_count; index++) {
        dest->tools[index] = src->tools[index];
    }
    dest->tool_count = src->tool_count;
    return HAL_OK;
}

void loki_config_set_defaults(loki_config_t *config)
{
    loki_tool_config_t *tool;

    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));

    copy_string(config->device_name, sizeof(config->device_name), "Loki");
    copy_string(config->dragon_name, sizeof(config->dragon_name), "Loki");
    copy_string(config->hostname, sizeof(config->hostname), "loki.local");
    copy_string(config->preferred_ssid, sizeof(config->preferred_ssid), "");

    config->dhcp_enabled = true;
    config->standalone_mode = false;
    config->announce_new_networks = true;
    config->web_ui_enabled = true;
    config->scan_interval_ms = 15000U;
    config->hop_interval_ms = 4000U;
    config->discovery_hold_ms = 20000U;
    copy_string(config->web.bind_address, sizeof(config->web.bind_address), "0.0.0.0");
    config->web.port = 8080U;

    config->plugins.display = true;
    config->plugins.dragon_ai = true;
    config->plugins.network_state = true;

    config->hardware.test_tft = false;
    config->hardware.test_flash = false;
    config->hardware.test_eeprom = false;
    config->hardware.test_flipper = false;
    copy_string(config->display.backend, sizeof(config->display.backend), "framebuffer");
    copy_string(config->display.framebuffer_device, sizeof(config->display.framebuffer_device), "/dev/fb1");

    config->ai.learning_rate = 0.16f;
    config->ai.actor_learning_rate = 0.14f;
    config->ai.critic_learning_rate = 0.22f;
    config->ai.discount_factor = 0.92f;
    config->ai.reward_decay = 0.99f;
    config->ai.entropy_beta = 0.02f;
    config->ai.policy_temperature = 0.68f;
    config->ai.tick_interval_ms = 120U;

    copy_string(config->dragon.profile, sizeof(config->dragon.profile), "guardian_egg");
    copy_string(config->dragon.temperament, sizeof(config->dragon.temperament), "curious");
    config->dragon.base_energy = 0.76f;
    config->dragon.base_curiosity = 0.64f;
    config->dragon.base_bond = 0.40f;
    config->dragon.base_hunger = 0.22f;
    config->dragon.hunger_rate = 0.015f;
    config->dragon.energy_decay = 0.010f;
    config->dragon.curiosity_rate = 0.012f;
    config->dragon.mood_hungry_threshold = 0.72f;
    config->dragon.mood_restful_energy_threshold = 0.28f;
    config->dragon.mood_curious_threshold = 0.68f;
    config->dragon.mood_excited_threshold = 0.70f;
    config->dragon.rest_bias = 0.35f;
    config->dragon.observe_bias = 0.25f;
    config->dragon.explore_bias = 0.45f;
    config->dragon.play_bias = 0.20f;
    config->dragon.rest_reward = 0.05f;
    config->dragon.observe_reward = 0.08f;
    config->dragon.explore_reward = 0.12f;
    config->dragon.play_reward = 0.15f;
    config->dragon.rest_energy_gain = 0.050f;
    config->dragon.rest_hunger_gain = 0.006f;
    config->dragon.observe_bond_gain = 0.015f;
    config->dragon.explore_curiosity_cost = 0.080f;
    config->dragon.explore_energy_cost = 0.030f;
    config->dragon.explore_xp_gain = 0.07f;
    config->dragon.play_bond_gain = 0.040f;
    config->dragon.play_energy_cost = 0.040f;
    config->dragon.play_hunger_gain = 0.020f;
    config->dragon.play_xp_gain = 0.10f;
    config->dragon.hunger_penalty_threshold = 0.80f;
    config->dragon.hunger_penalty = 0.20f;
    config->dragon.growth_interval_ms = 900000U;
    config->dragon.growth_xp_step = 90.0f;
    config->dragon.egg_stage_max = DRAGON_EGG_STAGE_MAX;
    config->dragon.hatchling_stage_max = DRAGON_HATCHLING_STAGE_MAX;

    if (loki_config_ensure_tool(config, "tool_wardrive", &tool) == HAL_OK) {
        tool->enabled = true;
        copy_string(tool->name, sizeof(tool->name), "Wardrive Journal");
        copy_string(tool->description, sizeof(tool->description), "Passive Wi-Fi scan logging with GPS-ready placeholders for later hardware.");
        copy_string(tool->command, sizeof(tool->command), "internal://wardrive");
    }

    if (loki_config_ensure_tool(config, "tool_status", &tool) == HAL_OK) {
        tool->enabled = true;
        copy_string(tool->name, sizeof(tool->name), "Status Feed");
        copy_string(tool->description, sizeof(tool->description), "Expose Loki runtime status and heartbeat data.");
        copy_string(tool->command, sizeof(tool->command), "internal://status");
    }

    if (loki_config_ensure_tool(config, "tool_loot", &tool) == HAL_OK) {
        tool->enabled = true;
        copy_string(tool->name, sizeof(tool->name), "Loot Journal");
        copy_string(tool->description, sizeof(tool->description), "Track growth milestones, trophies, and future inventory.");
        copy_string(tool->command, sizeof(tool->command), "internal://loot");
    }

    if (loki_config_ensure_tool(config, "tool_gps", &tool) == HAL_OK) {
        tool->enabled = true;
        copy_string(tool->name, sizeof(tool->name), "GPS Console");
        copy_string(tool->description, sizeof(tool->description), "Show live latitude, longitude, altitude, and GPS fix source when available.");
        copy_string(tool->command, sizeof(tool->command), "internal://gps");
    }

    if (loki_config_ensure_tool(config, "tool_peripherals", &tool) == HAL_OK) {
        tool->enabled = true;
        copy_string(tool->name, sizeof(tool->name), "Peripheral Console");
        copy_string(tool->description, sizeof(tool->description), "Summarize attached display, GPS state, nearby Wi-Fi, and safe peripheral probe options.");
        copy_string(tool->command, sizeof(tool->command), "internal://peripherals");
    }

    if (loki_config_ensure_tool(config, "tool_usb", &tool) == HAL_OK) {
        tool->enabled = false;
        copy_string(tool->name, sizeof(tool->name), "USB Probe");
        copy_string(tool->description, sizeof(tool->description), "List attached USB devices using the built-in USB probe.");
        copy_string(tool->command, sizeof(tool->command), "internal://usb");
    }

    if (loki_config_ensure_tool(config, "tool_i2c_probe", &tool) == HAL_OK) {
        tool->enabled = false;
        copy_string(tool->name, sizeof(tool->name), "I2C Probe");
        copy_string(tool->description, sizeof(tool->description), "List detected I2C buses using the built-in I2C probe.");
        copy_string(tool->command, sizeof(tool->command), "internal://i2c");
    }

    if (loki_config_ensure_tool(config, "tool_uart_probe", &tool) == HAL_OK) {
        tool->enabled = false;
        copy_string(tool->name, sizeof(tool->name), "UART Probe");
        copy_string(tool->description, sizeof(tool->description), "List detected UART-style device nodes using the built-in UART probe.");
        copy_string(tool->command, sizeof(tool->command), "internal://uart");
    }

    if (loki_config_ensure_tool(config, "tool_gpio_probe", &tool) == HAL_OK) {
        tool->enabled = false;
        copy_string(tool->name, sizeof(tool->name), "GPIO Probe");
        copy_string(tool->description, sizeof(tool->description), "Inspect GPIO line metadata using the built-in GPIO probe.");
        copy_string(tool->command, sizeof(tool->command), "internal://gpio");
    }

    if (loki_config_ensure_tool(config, "tool_nmap_ping", &tool) == HAL_OK) {
        tool->enabled = false;
        copy_string(tool->name, sizeof(tool->name), "Nmap Ping Sweep");
        copy_string(tool->description,
                    sizeof(tool->description),
                    "Validated nmap wrapper for host discovery. Edit the target query in loki.conf before enabling.");
        copy_string(tool->command,
                    sizeof(tool->command),
                    "internal://nmap?target=127.0.0.1&mode=ping");
    }

    if (loki_config_ensure_tool(config, "tool_custom_scan", &tool) == HAL_OK) {
        tool->enabled = false;
        copy_string(tool->name, sizeof(tool->name), "Custom Scan");
        copy_string(tool->description,
                    sizeof(tool->description),
                    "Validated curated scan profile wrapper. Edit target and ports in loki.conf before enabling.");
        copy_string(tool->command,
                    sizeof(tool->command),
                    "internal://custom_scan?target=127.0.0.1&profile=quick&ports=22,80");
    }
}

hal_status_t loki_config_save(const char *path, const loki_config_t *config)
{
    FILE *file;
    size_t index;

    if (path == NULL || config == NULL) {
        return HAL_INVALID_PARAM;
    }

    file = fopen(path, "w");
    if (file == NULL) {
        LOG_ERROR("Unable to write configuration file: %s", path);
        return HAL_ERROR;
    }

    fprintf(file,
            "; Loki runtime configuration\n"
            "; Edit this file to change device identity, timing, plugins, and AI behavior.\n"
            "; Add tools by creating new [tool_*] sections. There is no fixed tool count.\n\n"
            "[device]\n"
            "name=%s\n"
            "dragon_name=%s\n"
            "hostname=%s\n\n"
            "[network]\n"
            "dhcp_enabled=%s\n"
            "standalone_mode=%s\n"
            "announce_new_networks=%s\n"
            "preferred_ssid=%s\n"
            "scan_interval_ms=%u\n"
            "hop_interval_ms=%u\n"
            "discovery_hold_ms=%u\n"
            "web_ui_enabled=%s\n\n"
            "[web]\n"
            "bind_address=%s\n"
            "port=%u\n\n"
            "[plugins]\n"
            "display=%s\n"
            "dragon_ai=%s\n"
            "network_state=%s\n\n"
            "[hardware]\n"
            "test_tft=%s\n"
            "test_flash=%s\n"
            "test_eeprom=%s\n"
            "test_flipper=%s\n\n"
            "[display]\n"
            "backend=%s\n"
            "framebuffer_device=%s\n\n"
            "[ai]\n"
            "learning_rate=%.3f\n"
            "actor_learning_rate=%.3f\n"
            "critic_learning_rate=%.3f\n"
            "discount_factor=%.3f\n"
            "reward_decay=%.3f\n"
            "entropy_beta=%.3f\n"
            "policy_temperature=%.3f\n"
            "tick_interval_ms=%u\n\n"
            "[dragon]\n"
            "profile=%s\n"
            "temperament=%s\n"
            "base_energy=%.3f\n"
            "base_curiosity=%.3f\n"
            "base_bond=%.3f\n"
            "base_hunger=%.3f\n"
            "hunger_rate=%.3f\n"
            "energy_decay=%.3f\n"
            "curiosity_rate=%.3f\n"
            "mood_hungry_threshold=%.3f\n"
            "mood_restful_energy_threshold=%.3f\n"
            "mood_curious_threshold=%.3f\n"
            "mood_excited_threshold=%.3f\n"
            "rest_bias=%.3f\n"
            "observe_bias=%.3f\n"
            "explore_bias=%.3f\n"
            "play_bias=%.3f\n"
            "rest_reward=%.3f\n"
            "observe_reward=%.3f\n"
            "explore_reward=%.3f\n"
            "play_reward=%.3f\n"
            "rest_energy_gain=%.3f\n"
            "rest_hunger_gain=%.3f\n"
            "observe_bond_gain=%.3f\n"
            "explore_curiosity_cost=%.3f\n"
            "explore_energy_cost=%.3f\n"
            "explore_xp_gain=%.3f\n"
            "play_bond_gain=%.3f\n"
            "play_energy_cost=%.3f\n"
            "play_hunger_gain=%.3f\n"
            "play_xp_gain=%.3f\n"
            "hunger_penalty_threshold=%.3f\n"
            "hunger_penalty=%.3f\n"
            "growth_interval_ms=%u\n"
            "growth_xp_step=%.3f\n"
            "egg_stage_max=%u\n"
            "hatchling_stage_max=%u\n\n",
            config->device_name,
            config->dragon_name,
            config->hostname,
            config->dhcp_enabled ? "true" : "false",
            config->standalone_mode ? "true" : "false",
            config->announce_new_networks ? "true" : "false",
            config->preferred_ssid,
            config->scan_interval_ms,
            config->hop_interval_ms,
            config->discovery_hold_ms,
            config->web_ui_enabled ? "true" : "false",
            config->web.bind_address,
            config->web.port,
            config->plugins.display ? "true" : "false",
            config->plugins.dragon_ai ? "true" : "false",
            config->plugins.network_state ? "true" : "false",
            config->hardware.test_tft ? "true" : "false",
            config->hardware.test_flash ? "true" : "false",
            config->hardware.test_eeprom ? "true" : "false",
            config->hardware.test_flipper ? "true" : "false",
            config->display.backend,
            config->display.framebuffer_device,
            config->ai.learning_rate,
            config->ai.actor_learning_rate,
            config->ai.critic_learning_rate,
            config->ai.discount_factor,
            config->ai.reward_decay,
            config->ai.entropy_beta,
            config->ai.policy_temperature,
            config->ai.tick_interval_ms,
            config->dragon.profile,
            config->dragon.temperament,
            config->dragon.base_energy,
            config->dragon.base_curiosity,
            config->dragon.base_bond,
            config->dragon.base_hunger,
            config->dragon.hunger_rate,
            config->dragon.energy_decay,
            config->dragon.curiosity_rate,
            config->dragon.mood_hungry_threshold,
            config->dragon.mood_restful_energy_threshold,
            config->dragon.mood_curious_threshold,
            config->dragon.mood_excited_threshold,
            config->dragon.rest_bias,
            config->dragon.observe_bias,
            config->dragon.explore_bias,
            config->dragon.play_bias,
            config->dragon.rest_reward,
            config->dragon.observe_reward,
            config->dragon.explore_reward,
            config->dragon.play_reward,
            config->dragon.rest_energy_gain,
            config->dragon.rest_hunger_gain,
            config->dragon.observe_bond_gain,
            config->dragon.explore_curiosity_cost,
            config->dragon.explore_energy_cost,
            config->dragon.explore_xp_gain,
            config->dragon.play_bond_gain,
            config->dragon.play_energy_cost,
            config->dragon.play_hunger_gain,
            config->dragon.play_xp_gain,
            config->dragon.hunger_penalty_threshold,
            config->dragon.hunger_penalty,
            config->dragon.growth_interval_ms,
            config->dragon.growth_xp_step,
            config->dragon.egg_stage_max,
            config->dragon.hatchling_stage_max);

    for (index = 0; index < config->tool_count; index++) {
        const char *section = (config->tools[index].section[0] != '\0') ? config->tools[index].section : "tool_custom";

        fprintf(file,
                "[%s]\n"
                "enabled=%s\n"
                "name=%s\n"
                "description=%s\n"
                "command=%s\n\n",
                section,
                config->tools[index].enabled ? "true" : "false",
                config->tools[index].name,
                config->tools[index].description,
                config->tools[index].command);
    }

    fclose(file);
    return HAL_OK;
}

hal_status_t loki_config_load(const char *path, loki_config_t *config)
{
    FILE *file;
    char line[256];
    char section[64] = "";

    if (path == NULL || config == NULL) {
        return HAL_INVALID_PARAM;
    }

    loki_config_set_defaults(config);

    file = fopen(path, "r");
    if (file == NULL) {
        LOG_WARN("Configuration file not found, creating defaults at %s", path);
        return loki_config_save(path, config);
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *trimmed = trim_whitespace(line);
        char *equals;
        char *key;
        char *value;

        if (*trimmed == '\0' || *trimmed == ';' || *trimmed == '#') {
            continue;
        }

        if (*trimmed == '[') {
            char *close = strchr(trimmed, ']');
            if (close != NULL) {
                *close = '\0';
                copy_string(section, sizeof(section), trimmed + 1);
            }
            continue;
        }

        equals = strchr(trimmed, '=');
        if (equals == NULL) {
            continue;
        }

        *equals = '\0';
        key = trim_whitespace(trimmed);
        value = trim_whitespace(equals + 1);

        if (strcmp(section, "device") == 0) {
            if (strcmp(key, "name") == 0) {
                copy_string(config->device_name, sizeof(config->device_name), value);
            } else if (strcmp(key, "dragon_name") == 0) {
                copy_string(config->dragon_name, sizeof(config->dragon_name), value);
            } else if (strcmp(key, "hostname") == 0) {
                copy_string(config->hostname, sizeof(config->hostname), value);
            }
        } else if (strcmp(section, "network") == 0) {
            if (strcmp(key, "dhcp_enabled") == 0) {
                config->dhcp_enabled = parse_bool(value);
            } else if (strcmp(key, "standalone_mode") == 0) {
                config->standalone_mode = parse_bool(value);
            } else if (strcmp(key, "announce_new_networks") == 0) {
                config->announce_new_networks = parse_bool(value);
            } else if (strcmp(key, "preferred_ssid") == 0) {
                copy_string(config->preferred_ssid, sizeof(config->preferred_ssid), value);
            } else if (strcmp(key, "scan_interval_ms") == 0) {
                config->scan_interval_ms = (uint32_t)strtoul(value, NULL, 10);
            } else if (strcmp(key, "hop_interval_ms") == 0) {
                config->hop_interval_ms = (uint32_t)strtoul(value, NULL, 10);
            } else if (strcmp(key, "discovery_hold_ms") == 0) {
                config->discovery_hold_ms = (uint32_t)strtoul(value, NULL, 10);
            } else if (strcmp(key, "web_ui_enabled") == 0) {
                config->web_ui_enabled = parse_bool(value);
            }
        } else if (strcmp(section, "web") == 0) {
            if (strcmp(key, "bind_address") == 0) {
                copy_string(config->web.bind_address, sizeof(config->web.bind_address), value);
            } else if (strcmp(key, "port") == 0) {
                config->web.port = (uint16_t)strtoul(value, NULL, 10);
            }
        } else if (strcmp(section, "plugins") == 0) {
            if (strcmp(key, "display") == 0) {
                config->plugins.display = parse_bool(value);
            } else if (strcmp(key, "dragon_ai") == 0) {
                config->plugins.dragon_ai = parse_bool(value);
            } else if (strcmp(key, "network_state") == 0) {
                config->plugins.network_state = parse_bool(value);
            }
        } else if (strcmp(section, "hardware") == 0) {
            if (strcmp(key, "test_tft") == 0) {
                config->hardware.test_tft = parse_bool(value);
            } else if (strcmp(key, "test_flash") == 0) {
                config->hardware.test_flash = parse_bool(value);
            } else if (strcmp(key, "test_eeprom") == 0) {
                config->hardware.test_eeprom = parse_bool(value);
            } else if (strcmp(key, "test_flipper") == 0) {
                config->hardware.test_flipper = parse_bool(value);
            }
        } else if (strcmp(section, "display") == 0) {
            if (strcmp(key, "backend") == 0) {
                copy_string(config->display.backend, sizeof(config->display.backend), value);
            } else if (strcmp(key, "framebuffer_device") == 0) {
                copy_string(config->display.framebuffer_device, sizeof(config->display.framebuffer_device), value);
            }
        } else if (strncmp(section, "tool_", 5) == 0) {
            loki_tool_config_t *tool = NULL;

            if (loki_config_ensure_tool(config, section, &tool) != HAL_OK || tool == NULL) {
                continue;
            }

            if (strcmp(key, "enabled") == 0) {
                tool->enabled = parse_bool(value);
            } else if (strcmp(key, "name") == 0) {
                copy_string(tool->name, sizeof(tool->name), value);
            } else if (strcmp(key, "description") == 0) {
                copy_string(tool->description, sizeof(tool->description), value);
            } else if (strcmp(key, "command") == 0) {
                copy_string(tool->command, sizeof(tool->command), value);
            }
        } else if (strcmp(section, "ai") == 0) {
            if (strcmp(key, "learning_rate") == 0) {
                config->ai.learning_rate = strtof(value, NULL);
            } else if (strcmp(key, "actor_learning_rate") == 0) {
                config->ai.actor_learning_rate = strtof(value, NULL);
            } else if (strcmp(key, "critic_learning_rate") == 0) {
                config->ai.critic_learning_rate = strtof(value, NULL);
            } else if (strcmp(key, "discount_factor") == 0) {
                config->ai.discount_factor = strtof(value, NULL);
            } else if (strcmp(key, "reward_decay") == 0) {
                config->ai.reward_decay = strtof(value, NULL);
            } else if (strcmp(key, "entropy_beta") == 0) {
                config->ai.entropy_beta = strtof(value, NULL);
            } else if (strcmp(key, "policy_temperature") == 0) {
                config->ai.policy_temperature = strtof(value, NULL);
            } else if (strcmp(key, "tick_interval_ms") == 0) {
                config->ai.tick_interval_ms = (uint32_t)strtoul(value, NULL, 10);
            }
        } else if (strcmp(section, "dragon") == 0) {
            if (strcmp(key, "profile") == 0) {
                copy_string(config->dragon.profile, sizeof(config->dragon.profile), value);
            } else if (strcmp(key, "temperament") == 0) {
                copy_string(config->dragon.temperament, sizeof(config->dragon.temperament), value);
            } else if (strcmp(key, "base_energy") == 0) {
                config->dragon.base_energy = strtof(value, NULL);
            } else if (strcmp(key, "base_curiosity") == 0) {
                config->dragon.base_curiosity = strtof(value, NULL);
            } else if (strcmp(key, "base_bond") == 0) {
                config->dragon.base_bond = strtof(value, NULL);
            } else if (strcmp(key, "base_hunger") == 0) {
                config->dragon.base_hunger = strtof(value, NULL);
            } else if (strcmp(key, "hunger_rate") == 0) {
                config->dragon.hunger_rate = strtof(value, NULL);
            } else if (strcmp(key, "energy_decay") == 0) {
                config->dragon.energy_decay = strtof(value, NULL);
            } else if (strcmp(key, "curiosity_rate") == 0) {
                config->dragon.curiosity_rate = strtof(value, NULL);
            } else if (strcmp(key, "mood_hungry_threshold") == 0) {
                config->dragon.mood_hungry_threshold = strtof(value, NULL);
            } else if (strcmp(key, "mood_restful_energy_threshold") == 0) {
                config->dragon.mood_restful_energy_threshold = strtof(value, NULL);
            } else if (strcmp(key, "mood_curious_threshold") == 0) {
                config->dragon.mood_curious_threshold = strtof(value, NULL);
            } else if (strcmp(key, "mood_excited_threshold") == 0) {
                config->dragon.mood_excited_threshold = strtof(value, NULL);
            } else if (strcmp(key, "rest_bias") == 0) {
                config->dragon.rest_bias = strtof(value, NULL);
            } else if (strcmp(key, "observe_bias") == 0) {
                config->dragon.observe_bias = strtof(value, NULL);
            } else if (strcmp(key, "explore_bias") == 0) {
                config->dragon.explore_bias = strtof(value, NULL);
            } else if (strcmp(key, "play_bias") == 0) {
                config->dragon.play_bias = strtof(value, NULL);
            } else if (strcmp(key, "rest_reward") == 0) {
                config->dragon.rest_reward = strtof(value, NULL);
            } else if (strcmp(key, "observe_reward") == 0) {
                config->dragon.observe_reward = strtof(value, NULL);
            } else if (strcmp(key, "explore_reward") == 0) {
                config->dragon.explore_reward = strtof(value, NULL);
            } else if (strcmp(key, "play_reward") == 0) {
                config->dragon.play_reward = strtof(value, NULL);
            } else if (strcmp(key, "rest_energy_gain") == 0) {
                config->dragon.rest_energy_gain = strtof(value, NULL);
            } else if (strcmp(key, "rest_hunger_gain") == 0) {
                config->dragon.rest_hunger_gain = strtof(value, NULL);
            } else if (strcmp(key, "observe_bond_gain") == 0) {
                config->dragon.observe_bond_gain = strtof(value, NULL);
            } else if (strcmp(key, "explore_curiosity_cost") == 0) {
                config->dragon.explore_curiosity_cost = strtof(value, NULL);
            } else if (strcmp(key, "explore_energy_cost") == 0) {
                config->dragon.explore_energy_cost = strtof(value, NULL);
            } else if (strcmp(key, "explore_xp_gain") == 0) {
                config->dragon.explore_xp_gain = strtof(value, NULL);
            } else if (strcmp(key, "play_bond_gain") == 0) {
                config->dragon.play_bond_gain = strtof(value, NULL);
            } else if (strcmp(key, "play_energy_cost") == 0) {
                config->dragon.play_energy_cost = strtof(value, NULL);
            } else if (strcmp(key, "play_hunger_gain") == 0) {
                config->dragon.play_hunger_gain = strtof(value, NULL);
            } else if (strcmp(key, "play_xp_gain") == 0) {
                config->dragon.play_xp_gain = strtof(value, NULL);
            } else if (strcmp(key, "hunger_penalty_threshold") == 0) {
                config->dragon.hunger_penalty_threshold = strtof(value, NULL);
            } else if (strcmp(key, "hunger_penalty") == 0) {
                config->dragon.hunger_penalty = strtof(value, NULL);
            } else if (strcmp(key, "growth_interval_ms") == 0) {
                config->dragon.growth_interval_ms = (uint32_t)strtoul(value, NULL, 10);
            } else if (strcmp(key, "growth_xp_step") == 0) {
                config->dragon.growth_xp_step = strtof(value, NULL);
            } else if (strcmp(key, "egg_stage_max") == 0) {
                config->dragon.egg_stage_max = (uint32_t)strtoul(value, NULL, 10);
            } else if (strcmp(key, "hatchling_stage_max") == 0) {
                config->dragon.hatchling_stage_max = (uint32_t)strtoul(value, NULL, 10);
            }
        }
    }

    fclose(file);

    if (config->ai.actor_learning_rate <= 0.0f) {
        config->ai.actor_learning_rate = config->ai.learning_rate;
    }
    if (config->ai.critic_learning_rate <= 0.0f) {
        config->ai.critic_learning_rate = config->ai.learning_rate;
    }
    if (config->ai.policy_temperature <= 0.05f) {
        config->ai.policy_temperature = 0.05f;
    }

    LOG_INFO("Loaded configuration from %s", path);
    return HAL_OK;
}

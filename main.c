/**
 * @file main.c
 * @brief Loki - Orange Pi Zero 2W Interactive Display System
 * 
 * Main entry point and example usage of the Loki board system.
 * Demonstrates hardware initialization, device testing, and communication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

#include "dragon_ai.h"
#include "loki_config.h"
#include "platform.h"
#include "plugin_manager.h"
#include "system.h"
#include "tft_driver.h"
#include "sdcard_driver.h"
#include "flash_driver.h"
#include "eeprom_driver.h"
#include "flipper_uart.h"
#include "log.h"
#include "memory.h"
#include "retry.h"
#include "tool_runner.h"
#include "web_ui.h"

/* ===== GLOBAL STATE ===== */
volatile sig_atomic_t should_exit = 0;

typedef struct {
    loki_config_t config;
    dragon_ai_t dragon;
    loki_plugin_manager_t plugins;
    web_ui_t web_ui;
    uint32_t network_phase_ms;
    uint32_t heartbeat_ticks;
    uint8_t display_ready;
} app_state_t;

static app_state_t app_state = {0};

static const uint8_t g_font_digits[10][7] = {
    {14, 17, 19, 21, 25, 17, 14},
    {4, 12, 4, 4, 4, 4, 14},
    {14, 17, 1, 2, 4, 8, 31},
    {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},
    {31, 16, 16, 30, 1, 1, 30},
    {14, 16, 16, 30, 17, 17, 14},
    {31, 1, 2, 4, 8, 8, 8},
    {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15, 1, 1, 14}
};

static const uint8_t g_font_letters[26][7] = {
    {14, 17, 17, 31, 17, 17, 17},
    {30, 17, 17, 30, 17, 17, 30},
    {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30},
    {31, 16, 16, 30, 16, 16, 31},
    {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15},
    {17, 17, 17, 31, 17, 17, 17},
    {14, 4, 4, 4, 4, 4, 14},
    {1, 1, 1, 1, 17, 17, 14},
    {17, 18, 20, 24, 20, 18, 17},
    {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17},
    {17, 17, 25, 21, 19, 17, 17},
    {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16},
    {14, 17, 17, 17, 21, 18, 13},
    {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},
    {31, 4, 4, 4, 4, 4, 4},
    {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},
    {17, 17, 17, 21, 21, 21, 10},
    {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},
    {31, 1, 2, 4, 8, 16, 31}
};

static void build_dragon_profile(dragon_ai_profile_t *profile, const loki_config_t *config)
{
    size_t copy_length;

    if (profile == NULL || config == NULL) {
        return;
    }

    memset(profile, 0, sizeof(*profile));
    copy_length = strlen(config->dragon_name);
    if (copy_length >= sizeof(profile->name)) {
        copy_length = sizeof(profile->name) - 1U;
    }
    memcpy(profile->name, config->dragon_name, copy_length);
    profile->name[copy_length] = '\0';

    copy_length = strlen(config->dragon.profile);
    if (copy_length >= sizeof(profile->profile)) {
        copy_length = sizeof(profile->profile) - 1U;
    }
    memcpy(profile->profile, config->dragon.profile, copy_length);
    profile->profile[copy_length] = '\0';

    copy_length = strlen(config->dragon.temperament);
    if (copy_length >= sizeof(profile->temperament)) {
        copy_length = sizeof(profile->temperament) - 1U;
    }
    memcpy(profile->temperament, config->dragon.temperament, copy_length);
    profile->temperament[copy_length] = '\0';
    profile->learning_rate = config->ai.learning_rate;
    profile->actor_learning_rate = config->ai.actor_learning_rate;
    profile->critic_learning_rate = config->ai.critic_learning_rate;
    profile->discount_factor = config->ai.discount_factor;
    profile->reward_decay = config->ai.reward_decay;
    profile->entropy_beta = config->ai.entropy_beta;
    profile->policy_temperature = config->ai.policy_temperature;
    profile->base_energy = config->dragon.base_energy;
    profile->base_curiosity = config->dragon.base_curiosity;
    profile->base_bond = config->dragon.base_bond;
    profile->base_hunger = config->dragon.base_hunger;
    profile->hunger_rate = config->dragon.hunger_rate;
    profile->energy_decay = config->dragon.energy_decay;
    profile->curiosity_rate = config->dragon.curiosity_rate;
    profile->mood_hungry_threshold = config->dragon.mood_hungry_threshold;
    profile->mood_restful_energy_threshold = config->dragon.mood_restful_energy_threshold;
    profile->mood_curious_threshold = config->dragon.mood_curious_threshold;
    profile->mood_excited_threshold = config->dragon.mood_excited_threshold;
    profile->rest_bias = config->dragon.rest_bias;
    profile->observe_bias = config->dragon.observe_bias;
    profile->explore_bias = config->dragon.explore_bias;
    profile->play_bias = config->dragon.play_bias;
    profile->rest_reward = config->dragon.rest_reward;
    profile->observe_reward = config->dragon.observe_reward;
    profile->explore_reward = config->dragon.explore_reward;
    profile->play_reward = config->dragon.play_reward;
    profile->rest_energy_gain = config->dragon.rest_energy_gain;
    profile->rest_hunger_gain = config->dragon.rest_hunger_gain;
    profile->observe_bond_gain = config->dragon.observe_bond_gain;
    profile->explore_curiosity_cost = config->dragon.explore_curiosity_cost;
    profile->explore_energy_cost = config->dragon.explore_energy_cost;
    profile->explore_xp_gain = config->dragon.explore_xp_gain;
    profile->play_bond_gain = config->dragon.play_bond_gain;
    profile->play_energy_cost = config->dragon.play_energy_cost;
    profile->play_hunger_gain = config->dragon.play_hunger_gain;
    profile->play_xp_gain = config->dragon.play_xp_gain;
    profile->hunger_penalty_threshold = config->dragon.hunger_penalty_threshold;
    profile->hunger_penalty = config->dragon.hunger_penalty;
    profile->growth_interval_ms = config->dragon.growth_interval_ms;
    profile->growth_xp_step = config->dragon.growth_xp_step;
    profile->egg_stage_max = config->dragon.egg_stage_max;
    profile->hatchling_stage_max = config->dragon.hatchling_stage_max;
}

/* ===== SIGNAL HANDLERS ===== */
/**
 * @brief Signal handler for graceful shutdown
 */
static void handle_signal(int sig)
{
    LOG_WARN("Received signal %d, initiating shutdown...", sig);
    should_exit = 1;
}

/* ===== EXAMPLE: TFT DISPLAY TEST ===== */
/**
 * @brief Test TFT display functionality
 * 
 * Demonstrates display clearing and drawing colored rectangles.
 */
static void test_tft_display(void)
{
    LOG_INFO("Running TFT Display Test...");

    if (tft_clear() != HAL_OK) {
        app_state.display_ready = 0;
        LOG_ERROR("Failed to clear display");
        return;
    }

    app_state.display_ready = 1;

    /* Draw some patterns */
    tft_fill_rect(0, 0, 100, 100, RGB565(255, 0, 0));      /* Red square */
    tft_fill_rect(100, 0, 100, 100, RGB565(0, 255, 0));    /* Green square */
    tft_fill_rect(200, 0, 100, 100, RGB565(0, 0, 255));    /* Blue square */
    tft_fill_rect(300, 0, 180, 100, RGB565(255, 255, 0));  /* Yellow square */

    LOG_INFO("Display test complete");
}

static void sync_display_ready(void)
{
    if (tft_clear() == HAL_OK) {
        app_state.display_ready = 1;
        LOG_INFO("Display backend is ready for dashboard rendering");
        return;
    }

    app_state.display_ready = 0;
    LOG_WARN("Display backend is unavailable for dashboard rendering");
}

static color_t mood_accent_color(dragon_mood_t mood)
{
    switch (mood) {
        case DRAGON_MOOD_RESTFUL: return RGB565(88, 182, 255);
        case DRAGON_MOOD_ALERT: return RGB565(255, 173, 92);
        case DRAGON_MOOD_CURIOUS: return RGB565(114, 255, 184);
        case DRAGON_MOOD_HUNGRY: return RGB565(255, 99, 99);
        case DRAGON_MOOD_EXCITED: return RGB565(255, 221, 87);
        default: return COLOR_WHITE;
    }
}

static uint16_t bar_width_from_ratio(float ratio)
{
    if (ratio < 0.0f) {
        ratio = 0.0f;
    }
    if (ratio > 1.0f) {
        ratio = 1.0f;
    }
    return (uint16_t)(ratio * 120.0f);
}

static void copy_text(char *dest, size_t dest_size, const char *src)
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

static char sanitize_tool_char(char input)
{
    if ((input >= 'a' && input <= 'z') || (input >= '0' && input <= '9')) {
        return input;
    }
    if (input >= 'A' && input <= 'Z') {
        return (char)(input - 'A' + 'a');
    }
    return '_';
}

static void build_tool_section_name(char *dest, size_t dest_size, const char *raw_section, const char *raw_name)
{
    const char *source = (raw_section != NULL && raw_section[0] != '\0') ? raw_section : raw_name;
    size_t read_index = 0U;
    size_t write_index = 0U;
    bool last_was_separator = false;

    if (dest == NULL || dest_size < 8U) {
        return;
    }

    memcpy(dest, "tool_", 5U);
    write_index = 5U;

    if (source == NULL || source[0] == '\0') {
        copy_text(dest, dest_size, "tool_custom");
        return;
    }

    while (source[read_index] != '\0' && write_index < (dest_size - 1U)) {
        char mapped = sanitize_tool_char(source[read_index]);
        if (mapped == '_') {
            if (!last_was_separator) {
                dest[write_index++] = '_';
                last_was_separator = true;
            }
        } else {
            dest[write_index++] = mapped;
            last_was_separator = false;
        }
        read_index++;
    }

    if (write_index == 5U) {
        copy_text(dest, dest_size, "tool_custom");
        return;
    }

    dest[write_index] = '\0';
}

static const uint8_t *glyph_for_char(char character)
{
    static const uint8_t space[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t dot[7] = {0, 0, 0, 0, 0, 12, 12};
    static const uint8_t colon[7] = {0, 12, 12, 0, 12, 12, 0};
    static const uint8_t hyphen[7] = {0, 0, 0, 14, 0, 0, 0};
    static const uint8_t slash[7] = {1, 2, 4, 8, 16, 0, 0};
    static const uint8_t apostrophe[7] = {4, 4, 2, 0, 0, 0, 0};

    if (character >= '0' && character <= '9') {
        return g_font_digits[character - '0'];
    }
    if (character >= 'A' && character <= 'Z') {
        return g_font_letters[character - 'A'];
    }

    switch (character) {
        case '.': return dot;
        case ':': return colon;
        case '-': return hyphen;
        case '/': return slash;
        case '\'': return apostrophe;
        default: return space;
    }
}

static void draw_text_line(uint16_t x, uint16_t y, const char *text, color_t color, uint8_t scale)
{
    uint16_t cursor_x = x;
    size_t index;

    if (text == NULL || scale == 0U) {
        return;
    }

    for (index = 0; text[index] != '\0'; index++) {
        char character = text[index];
        const uint8_t *glyph;
        uint8_t row;
        uint8_t column;

        if (character >= 'a' && character <= 'z') {
            character = (char)(character - 'a' + 'A');
        }

        glyph = glyph_for_char(character);
        for (row = 0; row < 7U; row++) {
            for (column = 0; column < 5U; column++) {
                if ((glyph[row] & (1U << (4U - column))) != 0U) {
                    tft_fill_rect((uint16_t)(cursor_x + (column * scale)),
                                  (uint16_t)(y + (row * scale)),
                                  scale,
                                  scale,
                                  color);
                }
            }
        }

        cursor_x = (uint16_t)(cursor_x + (6U * scale));
    }
}

static void draw_text_block(uint16_t x,
                            uint16_t y,
                            uint16_t width,
                            const char *text,
                            color_t color,
                            uint8_t scale,
                            uint8_t max_lines)
{
    char line[96];
    size_t text_index = 0U;
    size_t max_chars;
    uint8_t line_index;

    if (text == NULL || scale == 0U || max_lines == 0U) {
        return;
    }

    max_chars = width / (6U * scale);
    if (max_chars == 0U) {
        return;
    }

    for (line_index = 0U; line_index < max_lines && text[text_index] != '\0'; line_index++) {
        size_t start_index = text_index;
        size_t line_length = 0U;
        size_t last_space = 0U;

        while (text[text_index] != '\0' && line_length < max_chars) {
            if (text[text_index] == ' ') {
                last_space = line_length;
            }
            line[line_length++] = text[text_index++];
        }

        if (text[text_index] != '\0' && line_length == max_chars && last_space > 0U) {
            text_index = start_index + last_space + 1U;
            line_length = last_space;
        }

        while (text[text_index] == ' ') {
            text_index++;
        }

        line[line_length] = '\0';
        draw_text_line(x, (uint16_t)(y + (line_index * (8U * scale))), line, color, scale);
    }
}

static void get_primary_ip_address(char *dest, size_t dest_size)
{
    if (dest == NULL || dest_size == 0U) {
        return;
    }

    copy_text(dest, dest_size, app_state.config.web.bind_address);

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
    {
        struct ifaddrs *ifaddr = NULL;
        struct ifaddrs *entry;

        if (getifaddrs(&ifaddr) != 0) {
            return;
        }

        for (entry = ifaddr; entry != NULL; entry = entry->ifa_next) {
            if (entry->ifa_addr == NULL || entry->ifa_addr->sa_family != AF_INET) {
                continue;
            }
            if ((entry->ifa_flags & IFF_LOOPBACK) != 0 || (entry->ifa_flags & IFF_UP) == 0) {
                continue;
            }

            if (inet_ntop(AF_INET,
                          &((struct sockaddr_in *)entry->ifa_addr)->sin_addr,
                          dest,
                          (socklen_t)dest_size) != NULL) {
                break;
            }
        }

        freeifaddrs(ifaddr);
    }
#endif
}

static int hex_value(char character)
{
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return 10 + (character - 'a');
    }
    if (character >= 'A' && character <= 'F') {
        return 10 + (character - 'A');
    }
    return -1;
}

static void url_decode(char *text)
{
    char *read_ptr = text;
    char *write_ptr = text;

    while (*read_ptr != '\0') {
        if (*read_ptr == '+') {
            *write_ptr++ = ' ';
            read_ptr++;
        } else if (*read_ptr == '%' && read_ptr[1] != '\0' && read_ptr[2] != '\0') {
            int high = hex_value(read_ptr[1]);
            int low = hex_value(read_ptr[2]);
            if (high >= 0 && low >= 0) {
                *write_ptr++ = (char)((high << 4) | low);
                read_ptr += 3;
            } else {
                *write_ptr++ = *read_ptr++;
            }
        } else {
            *write_ptr++ = *read_ptr++;
        }
    }

    *write_ptr = '\0';
}

static bool form_get_value(const char *form_data, const char *key, char *dest, size_t dest_size)
{
    const char *cursor = form_data;
    size_t key_length = strlen(key);

    while (cursor != NULL && *cursor != '\0') {
        const char *pair_end = strchr(cursor, '&');
        size_t pair_length = (pair_end == NULL) ? strlen(cursor) : (size_t)(pair_end - cursor);
        const char *equals = memchr(cursor, '=', pair_length);

        if (equals != NULL && (size_t)(equals - cursor) == key_length && strncmp(cursor, key, key_length) == 0) {
            size_t value_length = pair_length - ((size_t)(equals - cursor) + 1U);
            if (value_length >= dest_size) {
                value_length = dest_size - 1U;
            }
            memcpy(dest, equals + 1, value_length);
            dest[value_length] = '\0';
            url_decode(dest);
            return true;
        }

        if (pair_end == NULL) {
            break;
        }
        cursor = pair_end + 1;
    }

    if (dest_size > 0U) {
        dest[0] = '\0';
    }
    return false;
}

static hal_status_t save_settings_from_form(const char *form_data, char *message, size_t message_size, void *context)
{
    app_state_t *state = (app_state_t *)context;
    loki_config_t updated;
    char value[LOKI_TOOL_COMMAND_MAX];
    char new_tool_section[LOKI_CONFIG_STRING_MAX];
    char new_tool_name[LOKI_CONFIG_STRING_MAX];
    char new_tool_description[LOKI_TOOL_DESCRIPTION_MAX];
    char new_tool_command[LOKI_TOOL_COMMAND_MAX];
    size_t index;

    if (state == NULL || form_data == NULL || message == NULL || message_size == 0U) {
        return HAL_INVALID_PARAM;
    }

    if (loki_config_clone(&updated, &state->config) != HAL_OK) {
        snprintf(message, message_size, "Failed to prepare updated configuration.");
        return HAL_ERROR;
    }

    if (form_get_value(form_data, "dragon_name", value, sizeof(value))) {
        copy_text(updated.dragon_name, sizeof(updated.dragon_name), value);
        copy_text(updated.device_name, sizeof(updated.device_name), value);
    }
    if (form_get_value(form_data, "device_name", value, sizeof(value))) {
        copy_text(updated.device_name, sizeof(updated.device_name), value);
    }
    if (form_get_value(form_data, "hostname", value, sizeof(value))) {
        copy_text(updated.hostname, sizeof(updated.hostname), value);
    }
    if (form_get_value(form_data, "preferred_ssid", value, sizeof(value))) {
        copy_text(updated.preferred_ssid, sizeof(updated.preferred_ssid), value);
    }
    if (form_get_value(form_data, "scan_interval_ms", value, sizeof(value))) {
        updated.scan_interval_ms = (uint32_t)strtoul(value, NULL, 10);
    }
    if (form_get_value(form_data, "hop_interval_ms", value, sizeof(value))) {
        updated.hop_interval_ms = (uint32_t)strtoul(value, NULL, 10);
    }
    if (form_get_value(form_data, "web_bind_address", value, sizeof(value))) {
        copy_text(updated.web.bind_address, sizeof(updated.web.bind_address), value);
    }
    if (form_get_value(form_data, "web_port", value, sizeof(value))) {
        updated.web.port = (uint16_t)strtoul(value, NULL, 10);
    }
    if (form_get_value(form_data, "display_backend", value, sizeof(value))) {
        copy_text(updated.display.backend, sizeof(updated.display.backend), value);
    }
    if (form_get_value(form_data, "framebuffer_device", value, sizeof(value))) {
        copy_text(updated.display.framebuffer_device, sizeof(updated.display.framebuffer_device), value);
    }
    if (form_get_value(form_data, "learning_rate", value, sizeof(value))) {
        updated.ai.learning_rate = strtof(value, NULL);
    }
    if (form_get_value(form_data, "actor_learning_rate", value, sizeof(value))) {
        updated.ai.actor_learning_rate = strtof(value, NULL);
    }
    if (form_get_value(form_data, "critic_learning_rate", value, sizeof(value))) {
        updated.ai.critic_learning_rate = strtof(value, NULL);
    }
    if (form_get_value(form_data, "discount_factor", value, sizeof(value))) {
        updated.ai.discount_factor = strtof(value, NULL);
    }
    if (form_get_value(form_data, "reward_decay", value, sizeof(value))) {
        updated.ai.reward_decay = strtof(value, NULL);
    }
    if (form_get_value(form_data, "entropy_beta", value, sizeof(value))) {
        updated.ai.entropy_beta = strtof(value, NULL);
    }
    if (form_get_value(form_data, "policy_temperature", value, sizeof(value))) {
        updated.ai.policy_temperature = strtof(value, NULL);
    }
    if (form_get_value(form_data, "tick_interval_ms", value, sizeof(value))) {
        updated.ai.tick_interval_ms = (uint32_t)strtoul(value, NULL, 10);
    }

    updated.dhcp_enabled = form_get_value(form_data, "dhcp_enabled", value, sizeof(value));
    updated.web_ui_enabled = form_get_value(form_data, "web_ui_enabled", value, sizeof(value));
    updated.hardware.test_tft = form_get_value(form_data, "test_tft", value, sizeof(value));
    updated.hardware.test_flash = form_get_value(form_data, "test_flash", value, sizeof(value));
    updated.hardware.test_eeprom = form_get_value(form_data, "test_eeprom", value, sizeof(value));
    updated.hardware.test_flipper = form_get_value(form_data, "test_flipper", value, sizeof(value));

    for (index = 0; index < updated.tool_count; index++) {
        char key[32];

        snprintf(key, sizeof(key), "tool_%u_enabled", (unsigned int)(index + 1U));
        updated.tools[index].enabled = form_get_value(form_data, key, value, sizeof(value));

        snprintf(key, sizeof(key), "tool_%u_name", (unsigned int)(index + 1U));
        if (form_get_value(form_data, key, value, sizeof(value))) {
            copy_text(updated.tools[index].name, sizeof(updated.tools[index].name), value);
        }

        snprintf(key, sizeof(key), "tool_%u_description", (unsigned int)(index + 1U));
        if (form_get_value(form_data, key, value, sizeof(value))) {
            copy_text(updated.tools[index].description, sizeof(updated.tools[index].description), value);
        }

        snprintf(key, sizeof(key), "tool_%u_command", (unsigned int)(index + 1U));
        if (form_get_value(form_data, key, value, sizeof(value))) {
            copy_text(updated.tools[index].command, sizeof(updated.tools[index].command), value);
        }
    }

    new_tool_section[0] = '\0';
    new_tool_name[0] = '\0';
    new_tool_description[0] = '\0';
    new_tool_command[0] = '\0';
    form_get_value(form_data, "new_tool_section", new_tool_section, sizeof(new_tool_section));
    form_get_value(form_data, "new_tool_name", new_tool_name, sizeof(new_tool_name));
    form_get_value(form_data, "new_tool_description", new_tool_description, sizeof(new_tool_description));
    form_get_value(form_data, "new_tool_command", new_tool_command, sizeof(new_tool_command));

    if (new_tool_name[0] != '\0' || new_tool_command[0] != '\0' || new_tool_section[0] != '\0') {
        char section_name[LOKI_CONFIG_STRING_MAX];
        loki_tool_config_t *tool = NULL;

        build_tool_section_name(section_name, sizeof(section_name), new_tool_section, new_tool_name);
        if (loki_config_ensure_tool(&updated, section_name, &tool) == HAL_OK && tool != NULL) {
            tool->enabled = form_get_value(form_data, "new_tool_enabled", value, sizeof(value));
            if (new_tool_name[0] != '\0') {
                copy_text(tool->name, sizeof(tool->name), new_tool_name);
            }
            if (new_tool_description[0] != '\0') {
                copy_text(tool->description, sizeof(tool->description), new_tool_description);
            }
            if (new_tool_command[0] != '\0') {
                copy_text(tool->command, sizeof(tool->command), new_tool_command);
            }
        }
    }

    if (updated.scan_interval_ms == 0U) {
        updated.scan_interval_ms = 15000U;
    }
    if (updated.web.port == 0U) {
        updated.web.port = 8080U;
    }
    if (updated.ai.actor_learning_rate <= 0.0f) {
        updated.ai.actor_learning_rate = updated.ai.learning_rate;
    }
    if (updated.ai.critic_learning_rate <= 0.0f) {
        updated.ai.critic_learning_rate = updated.ai.learning_rate;
    }
    if (updated.ai.policy_temperature <= 0.05f) {
        updated.ai.policy_temperature = 0.05f;
    }

    loki_config_cleanup(&state->config);
    state->config = updated;
#if PLATFORM == PLATFORM_LINUX
    setenv("LOKI_DISPLAY_BACKEND", state->config.display.backend, 1);
    setenv("LOKI_FB_DEVICE", state->config.display.framebuffer_device, 1);
#endif

    if (loki_config_save(LOKI_CONFIG_PATH, &state->config) != HAL_OK) {
        snprintf(message, message_size, "Failed to save loki.conf.");
        return HAL_ERROR;
    }

    dragon_ai_apply_reward(&state->dragon, 0.08f);

    snprintf(message, message_size, "Saved loki.conf. Some display and web port changes take effect after restart.");
    return HAL_OK;
}

static hal_status_t run_tool_from_slot(size_t tool_index, char *output, size_t output_size, void *context)
{
    app_state_t *state = (app_state_t *)context;
    loki_tool_context_t tool_context;
    const dragon_ai_state_t *dragon;
    hal_status_t status;

    if (state == NULL || output == NULL || output_size == 0U || tool_index >= state->config.tool_count) {
        return HAL_INVALID_PARAM;
    }

    dragon = dragon_ai_get_state(&state->dragon);
    if (dragon == NULL) {
        snprintf(output, output_size, "Dragon state is unavailable.");
        return HAL_NOT_READY;
    }

    tool_context.config = &state->config;
    tool_context.dragon = dragon;
    tool_context.heartbeat_ticks = state->heartbeat_ticks;
    tool_context.network_phase_ms = state->network_phase_ms;

    status = loki_tool_run(&state->config.tools[tool_index], &tool_context, output, output_size);
    if (status == HAL_OK) {
        dragon_ai_apply_reward(&state->dragon, 0.12f);
    } else {
        dragon_ai_apply_reward(&state->dragon, -0.04f);
    }

    return status;
}

static void draw_dragon_egg(color_t accent)
{
    color_t shell_shadow = RGB565(58, 68, 84);
    color_t shell_mid = RGB565(112, 124, 138);
    color_t shell_light = RGB565(182, 190, 194);
    color_t shell_highlight = RGB565(226, 232, 230);
    color_t embryo_shadow = RGB565(26, 34, 42);
    color_t membrane = RGB565(76, 94, 102);

    tft_fill_rect(198, 82, 88, 132, shell_shadow);
    tft_fill_rect(206, 76, 72, 144, shell_mid);
    tft_fill_rect(216, 84, 52, 126, shell_light);
    tft_fill_rect(224, 94, 16, 96, shell_highlight);

    tft_fill_rect(226, 118, 22, 54, embryo_shadow);
    tft_fill_rect(218, 136, 16, 28, embryo_shadow);
    tft_fill_rect(240, 110, 10, 18, membrane);
    tft_fill_rect(248, 124, 12, 14, membrane);
    tft_fill_rect(246, 142, 10, 12, membrane);

    tft_fill_rect(222, 92, 6, 56, accent);
    tft_fill_rect(228, 120, 10, 4, accent);
    tft_fill_rect(236, 128, 6, 18, accent);
    tft_fill_rect(244, 116, 6, 12, accent);
    tft_fill_rect(250, 104, 4, 10, accent);
}

static void draw_dragon_hatchling(color_t accent, uint16_t pulse)
{
    tft_fill_rect(198, 122, 84, 52, RGB565(92, 212, 176));
    tft_fill_rect(214, 96, 52, 42, RGB565(112, 232, 194));
    tft_fill_rect(186, 130, 18, 34, RGB565(72, 170, 142));
    tft_fill_rect(276, 130, 18, 34, RGB565(72, 170, 142));
    tft_fill_rect(206, 106, 10, 12, accent);
    tft_fill_rect(264, 106, 10, 12, accent);
    tft_fill_rect(218, 112, 8, 10, COLOR_BLACK);
    tft_fill_rect(254, 112, 8, 10, COLOR_BLACK);
    tft_fill_rect(228, 142, 24, 8, RGB565(255, 236, 188));
    tft_fill_rect(194 - pulse, 118, 18, 26, RGB565(58, 126, 112));
    tft_fill_rect(268 + pulse, 118, 18, 26, RGB565(58, 126, 112));
    tft_fill_rect(214, 172, 12, 18, RGB565(72, 170, 142));
    tft_fill_rect(254, 172, 12, 18, RGB565(72, 170, 142));
}

static void draw_dragon_adult(color_t accent, uint16_t pulse)
{
    tft_fill_rect(194, 118, 92, 58, RGB565(72, 156, 214));
    tft_fill_rect(214, 88, 52, 44, RGB565(96, 184, 240));
    tft_fill_rect(170 - pulse, 102, 28, 58, RGB565(46, 92, 154));
    tft_fill_rect(282 + pulse, 102, 28, 58, RGB565(46, 92, 154));
    tft_fill_rect(206, 96, 12, 18, accent);
    tft_fill_rect(262, 96, 12, 18, accent);
    tft_fill_rect(220, 104, 8, 10, COLOR_BLACK);
    tft_fill_rect(252, 104, 8, 10, COLOR_BLACK);
    tft_fill_rect(228, 142, 28, 10, RGB565(255, 244, 192));
    tft_fill_rect(190, 170, 12, 34, RGB565(52, 118, 176));
    tft_fill_rect(278, 170, 12, 34, RGB565(52, 118, 176));
    tft_fill_rect(224, 78, 10, 18, RGB565(255, 238, 214));
    tft_fill_rect(246, 78, 10, 18, RGB565(255, 238, 214));
}

static void play_boot_animation(void)
{
    static const color_t accents[] = {
        RGB565(255, 173, 92),
        RGB565(114, 255, 184),
        RGB565(88, 182, 255),
        RGB565(255, 221, 87)
    };
    uint16_t step;

    if (!app_state.display_ready) {
        return;
    }

    for (step = 0U; step < 12U && !should_exit; step++) {
        color_t accent = accents[step % (sizeof(accents) / sizeof(accents[0]))];
        uint16_t progress = (uint16_t)(((step + 1U) * 296U) / 12U);
        uint16_t sweep = (uint16_t)(86U + (step * 18U));

        tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, RGB565(8, 12, 24));
        tft_fill_rect(0, 0, TFT_WIDTH, 8, accent);
        tft_fill_rect(60, 38, 360, 244, RGB565(10, 18, 30));
        tft_fill_rect(74, 54, 332, 120, RGB565(14, 22, 38));
        tft_fill_rect(74, 198, 332, 54, RGB565(14, 22, 38));
        tft_fill_rect(sweep, 62, 14, 104, RGB565(18, 42, 60));
        draw_text_line(152, 64, "LOKI", accent, 4);
        draw_text_line(128, 108, "AWAKENING", COLOR_WHITE, 2);
        draw_text_line(112, 208, "BOOTING DISPLAY AI AND WEB", COLOR_WHITE, 1);
        tft_fill_rect(92, 230, 296, 12, RGB565(36, 44, 58));
        tft_fill_rect(92, 230, progress, 12, accent);
        draw_dragon_egg(accent);
        DELAY_MS(90);
    }
}

static hal_status_t render_dragon_dashboard(const dragon_ai_state_t *dragon)
{
    color_t accent;
    char ip_address[32];
    char stage_line[48];
    char action_line[64];
    char ai_line[64];
    char scan_line[64];
    char reward_line[32];
    uint16_t shell_pulse;
    uint16_t pulse;
    uint16_t sweep_left;
    uint16_t sweep_top;
    uint16_t scan_progress;
    bool cursor_visible;

    if (dragon == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (!app_state.display_ready) {
        return HAL_OK;
    }

    accent = mood_accent_color(dragon->mood);
    pulse = (uint16_t)((dragon->age_ticks % 12) * 2);
    shell_pulse = (uint16_t)(6U + ((dragon->age_ticks % 8U) * 2U));
    sweep_left = (uint16_t)(30U + ((dragon->age_ticks * 11U) % 220U));
    sweep_top = (uint16_t)(56U + ((dragon->age_ticks * 7U) % 96U));
    scan_progress = 0U;
    if (app_state.config.scan_interval_ms > 0U) {
        scan_progress = (uint16_t)((app_state.network_phase_ms * 144U) / app_state.config.scan_interval_ms);
        if (scan_progress > 144U) {
            scan_progress = 144U;
        }
    }
    cursor_visible = ((dragon->age_ticks % 4U) < 2U);
    get_primary_ip_address(ip_address, sizeof(ip_address));
    snprintf(stage_line,
             sizeof(stage_line),
             "%s STAGE %u",
             dragon_ai_life_stage_name(dragon->growth_stage),
             (unsigned int)dragon->growth_stage);
    snprintf(action_line,
             sizeof(action_line),
             "MOOD %s ACTION %s",
             dragon_ai_mood_name(dragon->mood),
             dragon_ai_action_name(dragon->action));
    snprintf(ai_line,
             sizeof(ai_line),
             "CONF %.2f ADV %.2f",
             dragon->policy_confidence,
             dragon->last_advantage);
    snprintf(scan_line,
             sizeof(scan_line),
             "SCAN %u OF %u MS",
             (unsigned int)app_state.network_phase_ms,
             (unsigned int)app_state.config.scan_interval_ms);
    snprintf(reward_line,
             sizeof(reward_line),
             "REWARD %.2f",
             dragon->last_reward);

    if (tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, RGB565(8, 12, 24)) != HAL_OK) {
        return HAL_OK;
    }

    tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, RGB565(8, 12, 24));
    tft_fill_rect(0, 0, TFT_WIDTH, 6, accent);

    tft_fill_rect(18, 18, 270, 184, RGB565(12, 18, 30));
    tft_fill_rect(300, 18, 162, 96, RGB565(14, 24, 40));
    tft_fill_rect(300, 126, 162, 176, RGB565(20, 18, 32));
    tft_fill_rect(18, 216, 270, 86, RGB565(14, 20, 34));

    tft_fill_rect((uint16_t)(126U - shell_pulse),
                  (uint16_t)(58U - (shell_pulse / 2U)),
                  (uint16_t)(56U + (shell_pulse * 2U)),
                  (uint16_t)(130U + shell_pulse),
                  RGB565(18, 38, 56));
    tft_fill_rect(sweep_left, 44, 12, 130, RGB565(16, 42, 60));
    tft_fill_rect(38, sweep_top, 220, 8, RGB565(14, 32, 52));

    draw_text_line(308, 28, "IP ADDRESS", COLOR_WHITE, 2);
    draw_text_line(310, 58, ip_address, accent, 2);

    draw_text_line(308, 136, "LOKI DIALOG", COLOR_WHITE, 2);
    draw_text_line(308, 164, stage_line, accent, 1);
    draw_text_line(308, 180, action_line, COLOR_WHITE, 1);
    draw_text_line(308, 196, ai_line, RGB565(148, 214, 255), 1);
    draw_text_line(308, 212, scan_line, RGB565(124, 255, 194), 1);
    draw_text_line(308, 228, reward_line, RGB565(255, 202, 112), 1);
    draw_text_block(308, 248, 144, dragon->speech, COLOR_WHITE, 1, 4);
    tft_fill_rect(308, 286, 144, 6, RGB565(34, 42, 54));
    tft_fill_rect(308, 286, scan_progress, 6, accent);
    if (cursor_visible) {
        tft_fill_rect(446, 292, 6, 2, accent);
    }

    tft_fill_rect(126, 58, 56, 130, RGB565(18, 26, 42));
    tft_fill_rect(28, 58, 84, 130, RGB565(18, 26, 42));
    tft_fill_rect(190, 58, 84, 130, RGB565(18, 26, 42));
    switch (dragon_ai_life_stage(dragon->growth_stage)) {
        case DRAGON_LIFE_STAGE_EGG:
            draw_dragon_egg(accent);
            break;
        case DRAGON_LIFE_STAGE_HATCHLING:
            draw_dragon_hatchling(accent, pulse);
            break;
        case DRAGON_LIFE_STAGE_ADULT:
        default:
            draw_dragon_adult(accent, pulse);
            break;
    }

    draw_text_line(30, 228, "HUNGER", COLOR_WHITE, 1);
    tft_fill_rect(30, 244, 116, 10, RGB565(40, 50, 66));
    tft_fill_rect(30, 244, bar_width_from_ratio(1.0f - dragon->hunger), 10, RGB565(255, 120, 110));
    draw_text_line(30, 260, "ENERGY", COLOR_WHITE, 1);
    tft_fill_rect(30, 276, 116, 10, RGB565(40, 50, 66));
    tft_fill_rect(30, 276, bar_width_from_ratio(dragon->energy), 10, RGB565(120, 215, 255));

    draw_text_line(160, 228, "CURIOUS", COLOR_WHITE, 1);
    tft_fill_rect(160, 244, 116, 10, RGB565(40, 50, 66));
    tft_fill_rect(160, 244, bar_width_from_ratio(dragon->curiosity), 10, RGB565(125, 255, 184));
    draw_text_line(160, 260, "BOND", COLOR_WHITE, 1);
    tft_fill_rect(160, 276, 116, 10, RGB565(40, 50, 66));
    tft_fill_rect(160, 276, bar_width_from_ratio(dragon->bond), 10, RGB565(255, 221, 87));

    return HAL_OK;
}

static hal_status_t display_plugin_init(loki_plugin_t *plugin)
{
    (void)plugin;
    return HAL_OK;
}

static hal_status_t display_plugin_update(loki_plugin_t *plugin, uint32_t tick_ms)
{
    (void)plugin;
    (void)tick_ms;

    return render_dragon_dashboard(dragon_ai_get_state(&app_state.dragon));
}

static hal_status_t display_plugin_shutdown(loki_plugin_t *plugin)
{
    (void)plugin;
    return HAL_OK;
}

static hal_status_t dragon_plugin_init(loki_plugin_t *plugin)
{
    dragon_ai_profile_t profile;

    (void)plugin;
    build_dragon_profile(&profile, &app_state.config);
    dragon_ai_init(&app_state.dragon, &profile);
    return HAL_OK;
}

static hal_status_t dragon_plugin_update(loki_plugin_t *plugin, uint32_t tick_ms)
{
    (void)plugin;
    dragon_ai_tick(&app_state.dragon, tick_ms);
    return HAL_OK;
}

static hal_status_t dragon_plugin_shutdown(loki_plugin_t *plugin)
{
    (void)plugin;
    return HAL_OK;
}

static hal_status_t network_state_plugin_init(loki_plugin_t *plugin)
{
    (void)plugin;
    app_state.network_phase_ms = 0;
    LOG_INFO("Network state plugin active (dhcp=%s, scan=%u ms, hop=%u ms)",
             app_state.config.dhcp_enabled ? "on" : "off",
             app_state.config.scan_interval_ms,
             app_state.config.hop_interval_ms);
    return HAL_OK;
}

static hal_status_t network_state_plugin_update(loki_plugin_t *plugin, uint32_t tick_ms)
{
    (void)plugin;

    app_state.network_phase_ms += tick_ms;
    if (app_state.config.scan_interval_ms > 0 &&
        app_state.network_phase_ms >= app_state.config.scan_interval_ms) {
        app_state.network_phase_ms = 0;
        LOG_DEBUG("Network scheduler heartbeat: preferred_ssid='%s' dhcp=%s web_ui=%s",
                  app_state.config.preferred_ssid,
                  app_state.config.dhcp_enabled ? "true" : "false",
                  app_state.config.web_ui_enabled ? "true" : "false");
        dragon_ai_apply_reward(&app_state.dragon, 0.05f);
    }

    return HAL_OK;
}

static hal_status_t network_state_plugin_shutdown(loki_plugin_t *plugin)
{
    (void)plugin;
    return HAL_OK;
}

static hal_status_t web_ui_plugin_init(loki_plugin_t *plugin)
{
    (void)plugin;

    if (web_ui_init(&app_state.web_ui,
                    app_state.config.web.bind_address,
                    app_state.config.web.port) != HAL_OK) {
        LOG_WARN("Web UI initialization failed");
        return HAL_ERROR;
    }

    web_ui_set_handlers(&app_state.web_ui,
                        save_settings_from_form,
                        run_tool_from_slot,
                        &app_state);

    if (web_ui_start(&app_state.web_ui) != HAL_OK) {
        LOG_WARN("Web UI start failed");
        web_ui_stop(&app_state.web_ui);
        return HAL_ERROR;
    }

    return HAL_OK;
}

static hal_status_t web_ui_plugin_update(loki_plugin_t *plugin, uint32_t tick_ms)
{
    web_ui_snapshot_t snapshot;
    const dragon_ai_state_t *dragon;
    char ip_address[32];

    (void)plugin;
    (void)tick_ms;

    memset(&snapshot, 0, sizeof(snapshot));
    dragon = dragon_ai_get_state(&app_state.dragon);
    if (dragon == NULL) {
        return HAL_OK;
    }

    get_primary_ip_address(ip_address, sizeof(ip_address));

    copy_text(snapshot.device_name, sizeof(snapshot.device_name), app_state.config.device_name);
    copy_text(snapshot.dragon_name, sizeof(snapshot.dragon_name), app_state.config.dragon_name);
    copy_text(snapshot.hostname, sizeof(snapshot.hostname), app_state.config.hostname);
    copy_text(snapshot.primary_ip, sizeof(snapshot.primary_ip), ip_address);
    copy_text(snapshot.preferred_ssid, sizeof(snapshot.preferred_ssid), app_state.config.preferred_ssid);
    copy_text(snapshot.bind_address, sizeof(snapshot.bind_address), app_state.config.web.bind_address);
    copy_text(snapshot.display_backend, sizeof(snapshot.display_backend), app_state.config.display.backend);
    copy_text(snapshot.framebuffer_device, sizeof(snapshot.framebuffer_device), app_state.config.display.framebuffer_device);
    snapshot.port = app_state.config.web.port;
    snapshot.heartbeat_ticks = app_state.heartbeat_ticks;
    snapshot.network_phase_ms = app_state.network_phase_ms;
    snapshot.scan_interval_ms = app_state.config.scan_interval_ms;
    snapshot.hop_interval_ms = app_state.config.hop_interval_ms;
    snapshot.ai_tick_interval_ms = app_state.config.ai.tick_interval_ms;
    snapshot.dhcp_enabled = app_state.config.dhcp_enabled;
    snapshot.web_ui_enabled = app_state.config.web_ui_enabled;
    snapshot.display_ready = (app_state.display_ready != 0U);
    snapshot.test_tft = app_state.config.hardware.test_tft;
    snapshot.test_flash = app_state.config.hardware.test_flash;
    snapshot.test_eeprom = app_state.config.hardware.test_eeprom;
    snapshot.test_flipper = app_state.config.hardware.test_flipper;
    snapshot.ai_learning_rate = app_state.config.ai.learning_rate;
    snapshot.ai_actor_learning_rate = app_state.config.ai.actor_learning_rate;
    snapshot.ai_critic_learning_rate = app_state.config.ai.critic_learning_rate;
    snapshot.ai_discount_factor = app_state.config.ai.discount_factor;
    snapshot.ai_reward_decay = app_state.config.ai.reward_decay;
    snapshot.ai_entropy_beta = app_state.config.ai.entropy_beta;
    snapshot.ai_policy_temperature = app_state.config.ai.policy_temperature;
    snapshot.tools = app_state.config.tools;
    snapshot.tool_count = app_state.config.tool_count;
    snapshot.dragon = *dragon;

    return web_ui_update_snapshot(&app_state.web_ui, &snapshot);
}

static hal_status_t web_ui_plugin_shutdown(loki_plugin_t *plugin)
{
    (void)plugin;
    return web_ui_stop(&app_state.web_ui);
}

static void register_runtime_plugins(void)
{
    loki_plugin_t display_plugin = {
        .name = "display",
        .enabled = app_state.config.plugins.display,
        .context = NULL,
        .init = display_plugin_init,
        .update = display_plugin_update,
        .shutdown = display_plugin_shutdown,
    };
    loki_plugin_t dragon_plugin = {
        .name = "dragon_ai",
        .enabled = app_state.config.plugins.dragon_ai,
        .context = NULL,
        .init = dragon_plugin_init,
        .update = dragon_plugin_update,
        .shutdown = dragon_plugin_shutdown,
    };
    loki_plugin_t network_plugin = {
        .name = "network_state",
        .enabled = app_state.config.plugins.network_state,
        .context = NULL,
        .init = network_state_plugin_init,
        .update = network_state_plugin_update,
        .shutdown = network_state_plugin_shutdown,
    };
    loki_plugin_t web_plugin = {
        .name = "web_ui",
        .enabled = app_state.config.web_ui_enabled,
        .context = NULL,
        .init = web_ui_plugin_init,
        .update = web_ui_plugin_update,
        .shutdown = web_ui_plugin_shutdown,
    };

    loki_plugin_manager_init(&app_state.plugins);
    loki_plugin_manager_register(&app_state.plugins, &dragon_plugin);
    loki_plugin_manager_register(&app_state.plugins, &network_plugin);
    loki_plugin_manager_register(&app_state.plugins, &display_plugin);
    loki_plugin_manager_register(&app_state.plugins, &web_plugin);
}

/* ===== EXAMPLE: EEPROM READ/WRITE TEST ===== */
/**
 * @brief Test EEPROM read/write with verification
 * 
 * Writes test data to EEPROM, reads it back, and verifies integrity.
 */
static void test_eeprom(void)
{
    LOG_INFO("Running EEPROM Test...");

    uint8_t write_data[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    uint8_t read_data[8] = {0};

    /* Write to EEPROM with retry logic */
    hal_status_t status = RETRY(eeprom_write(0, write_data, 8), RETRY_BALANCED);
    
    if (status != HAL_OK) {
        LOG_ERROR("Failed to write EEPROM after retries");
        return;
    }
    LOG_INFO("Written 8 bytes to EEPROM address 0");

    DELAY_MS(1000);  /* Wait for write to complete */

    /* Read from EEPROM */
    status = RETRY(eeprom_read(0, read_data, 8), RETRY_BALANCED);
    
    if (status != HAL_OK) {
        LOG_ERROR("Failed to read EEPROM");
        return;
    }

    /* Verify */
    LOG_INFO("Read from EEPROM: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
            read_data[0], read_data[1], read_data[2], read_data[3],
            read_data[4], read_data[5], read_data[6], read_data[7]);

    if (memcmp(write_data, read_data, 8) == 0) {
        LOG_INFO("✓ EEPROM test PASSED");
    } else {
        LOG_ERROR("✗ EEPROM test FAILED (data mismatch)");
    }
}

/* ===== EXAMPLE: FLASH MEMORY TEST ===== */
/**
 * @brief Test Flash memory JEDEC ID
 * 
 * Reads and verifies the JEDEC ID from the W25Q40 Flash chip.
 */
static void test_flash(void)
{
    LOG_INFO("Running Flash Memory Test...");

    uint8_t jedec_id[3];
    hal_status_t status = RETRY(flash_get_jedec_id(jedec_id), RETRY_BALANCED);
    
    if (status == HAL_OK) {
        LOG_INFO("Flash JEDEC ID: 0x%02X%02X%02X", jedec_id[0], jedec_id[1], jedec_id[2]);
        
        if ((jedec_id[0] << 16 | jedec_id[1] << 8 | jedec_id[2]) == FLASH_JEDEC_ID) {
            LOG_INFO("✓ Flash identification verified");
        } else {
            LOG_WARN("Flash JEDEC ID mismatch (expected: 0x%06X)", FLASH_JEDEC_ID);
        }
    } else {
        LOG_ERROR("Failed to read Flash JEDEC ID");
    }
}

/* ===== EXAMPLE: FLIPPER UART TEST ===== */
/**
 * @brief Test Flipper UART connectivity
 * 
 * Checks if Flipper is connected and responsive.
 */
static void test_flipper_communication(void)
{
    LOG_INFO("Running Flipper UART Test...");

    if (flipper_available() > 0) {
        LOG_INFO("✓ Flipper data available");
    } else {
        LOG_WARN("No Flipper data available (is Flipper connected?)");
    }
}

/* ===== MAIN APPLICATION ===== */
/**
 * @brief Main application entry point
 * 
 * Initializes the system, runs hardware tests, and waits for user input.
 * 
 * @param[in] argc Argument count
 * @param[in] argv Argument values
 * @return EXIT_SUCCESS on normal shutdown, EXIT_FAILURE on error
 */
int main(int argc, char *argv[])
{
    (void)argc;  /* Suppress unused parameter warning */
    (void)argv;  /* Suppress unused parameter warning */
    /* Print banner */
    fprintf(stdout, "╔════════════════════════════════════════════════════╗\n");
    fprintf(stdout, "║        Loki - Orange Pi Zero 2W Display System    ║\n");
    fprintf(stdout, "║         Powered by Flipper Zero Integration       ║\n");
    fprintf(stdout, "╚════════════════════════════════════════════════════╝\n\n");

    /* Initialize logging system */
    log_init();

    if (loki_config_load(LOKI_CONFIG_PATH, &app_state.config) != HAL_OK) {
        LOG_WARN("Continuing with in-memory defaults only");
        loki_config_set_defaults(&app_state.config);
    }
    if (app_state.config.ai.tick_interval_ms < 50U) {
        app_state.config.ai.tick_interval_ms = 50U;
    }
    if (app_state.config.scan_interval_ms == 0U) {
        app_state.config.scan_interval_ms = 15000U;
    }

#if PLATFORM == PLATFORM_LINUX
    setenv("LOKI_DISPLAY_BACKEND", app_state.config.display.backend, 1);
    setenv("LOKI_FB_DEVICE", app_state.config.display.framebuffer_device, 1);
#endif
    
#ifdef DEBUG
    log_set_level(LOG_DEBUG);
    LOG_DEBUG("Debug mode enabled");
#else
    log_set_level(LOG_INFO);
    LOG_INFO("Release mode");
#endif

#if PLATFORM == PLATFORM_LINUX
    /* Set up signal handlers for graceful shutdown */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif

    /* Initialize system */
    if (system_init() != HAL_OK) {
        LOG_CRITICAL("System initialization failed!");
        return EXIT_FAILURE;
    }

    sync_display_ready();
    play_boot_animation();

    register_runtime_plugins();
    loki_plugin_manager_start(&app_state.plugins);

    /* Run hardware tests */
    LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    LOG_INFO("Running hardware tests...");
    LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    if (app_state.config.hardware.test_tft) {
        test_tft_display();
        DELAY_MS(1000);
    } else {
        LOG_INFO("Skipping TFT Display Test (hardware.test_tft=false)");
    }

    if (app_state.config.hardware.test_flash) {
        test_flash();
        DELAY_MS(1000);
    } else {
        LOG_INFO("Skipping Flash Memory Test (hardware.test_flash=false)");
    }

    if (app_state.config.hardware.test_eeprom) {
        test_eeprom();
        DELAY_MS(1000);
    } else {
        LOG_INFO("Skipping EEPROM Test (hardware.test_eeprom=false)");
    }

    if (app_state.config.hardware.test_flipper) {
        test_flipper_communication();
        DELAY_MS(1000);
    } else {
        LOG_INFO("Skipping Flipper UART Test (hardware.test_flipper=false)");
    }

    dragon_ai_apply_reward(&app_state.dragon, 0.20f);

    LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    LOG_INFO("Loki: %s | System label: %s | Hostname: %s",
             app_state.config.dragon_name,
             app_state.config.device_name,
             app_state.config.hostname);
    LOG_INFO("Plugins: display=%s dragon_ai=%s network_state=%s",
             app_state.config.plugins.display ? "on" : "off",
             app_state.config.plugins.dragon_ai ? "on" : "off",
             app_state.config.plugins.network_state ? "on" : "off");

    /* Main loop */
    LOG_INFO("Entering main loop. Press Ctrl+C to exit.");
    LOG_INFO("Waiting for Flipper commands...\n");

    while (!should_exit) {
        loki_plugin_manager_update(&app_state.plugins, app_state.config.ai.tick_interval_ms);
        app_state.heartbeat_ticks++;

        /* Check for Flipper messages */
        if (flipper_available() > 0) {
            flipper_message_t msg = {0};
            
            if (flipper_receive_message(&msg, 100) == HAL_OK) {
                LOG_INFO("Received Flipper command: 0x%02X (length: %d)", 
                        msg.cmd, msg.length);

                /* Send acknowledgment */
                flipper_message_t ack = {
                    .cmd = FLIPPER_CMD_ACK,
                    .length = 1,
                    .payload = (uint8_t *)&msg.cmd,
                };
                flipper_send_message(&ack);
                dragon_ai_apply_reward(&app_state.dragon, 0.35f);

                /* Free payload if allocated */
                if (msg.payload != NULL) {
                    free(msg.payload);
                }
            }
        }

        DELAY_MS(app_state.config.ai.tick_interval_ms);  /* Keep runtime behavior config-driven */
    }

    /* Shutdown */
    LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    LOG_INFO("Initiating system shutdown...");
    loki_plugin_manager_stop(&app_state.plugins);
    system_shutdown();
    
    LOG_INFO("Loki system terminated successfully");
    fprintf(stdout, "\n✓ Goodbye!\n");
    
    return EXIT_SUCCESS;
}

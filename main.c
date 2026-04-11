/**
 * @file main.c
 * @brief Loki - Orange Pi Zero 2W Interactive Display System
 * 
 * Main entry point and example usage of the Loki board system.
 * Demonstrates hardware initialization, device testing, and communication.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <math.h>
#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/stat.h>
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

#define LOKI_NEARBY_NETWORKS_MAX 256
#define LOKI_DISCOVERY_MESSAGE_MAX 256
#define LOKI_KNOWN_NETWORKS_MAX 512
#define LOKI_NETWORK_CACHE_PATH "/var/lib/loki/known_networks.cache"

/* ===== GLOBAL STATE ===== */
volatile sig_atomic_t should_exit = 0;

typedef struct {
    loki_config_t config;
    dragon_ai_t dragon;
    loki_gps_state_t gps;
    char nearby_networks[LOKI_NEARBY_NETWORKS_MAX];
    char discovery_message[LOKI_DISCOVERY_MESSAGE_MAX];
    char known_networks[LOKI_KNOWN_NETWORKS_MAX];
    uint32_t nearby_network_count;
    uint32_t discovery_hold_remaining_ms;
    loki_plugin_manager_t plugins;
    web_ui_t web_ui;
    uint32_t network_phase_ms;
    uint32_t heartbeat_ticks;
    uint8_t display_ready;
    float credits_balance;
    uint32_t running_tool_count;
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
static hal_status_t run_tool_from_slot(size_t tool_index,
                                       bool request_is_localhost,
                                       const char *remote_address,
                                       char *output,
                                       size_t output_size,
                                       void *context);
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

static bool network_list_contains(const char *list, const char *ssid)
{
    const char *cursor;

    if (list == NULL || ssid == NULL || ssid[0] == '\0') {
        return false;
    }

    cursor = list;
    while (*cursor != '\0') {
        char token[96];
        size_t length = 0U;

        while (*cursor == ' ' || *cursor == '|') {
            cursor++;
        }
        while (*cursor != '\0' && *cursor != '|' && length < (sizeof(token) - 1U)) {
            token[length++] = *cursor++;
        }
        while (length > 0U && token[length - 1U] == ' ') {
            length--;
        }
        token[length] = '\0';
        if (token[0] != '\0' && strcmp(token, ssid) == 0) {
            return true;
        }
        while (*cursor == ' ') {
            cursor++;
        }
        if (*cursor == '|') {
            cursor++;
        }
    }

    return false;
}

static uint32_t count_network_list_entries(const char *list)
{
    const char *cursor;
    uint32_t count = 0U;

    if (list == NULL || list[0] == '\0') {
        return 0U;
    }

    cursor = list;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '|') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        count++;
        while (*cursor != '\0' && *cursor != '|') {
            cursor++;
        }
    }

    return count;
}

static void set_discovery_message(const char *message, uint32_t hold_ms)
{
    if (message == NULL || message[0] == '\0') {
        app_state.discovery_message[0] = '\0';
        app_state.discovery_hold_remaining_ms = 0U;
        return;
    }

    copy_text(app_state.discovery_message, sizeof(app_state.discovery_message), message);
    app_state.discovery_hold_remaining_ms = hold_ms;
}

static void load_known_network_cache(void)
{
    app_state.known_networks[0] = '\0';

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
    {
        FILE *file = fopen(LOKI_NETWORK_CACHE_PATH, "r");

        if (file != NULL) {
            size_t bytes_read = fread(app_state.known_networks,
                                      1U,
                                      sizeof(app_state.known_networks) - 1U,
                                      file);

            app_state.known_networks[bytes_read] = '\0';
            app_state.known_networks[strcspn(app_state.known_networks, "\r\n")] = '\0';
            fclose(file);
        }
    }
#endif
}

static void save_known_network_cache(void)
{
#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
    {
        FILE *file;

        if (mkdir("/var/lib/loki", 0755) != 0 && errno != EEXIST) {
            LOG_WARN("Unable to create network cache directory: %s", strerror(errno));
            return;
        }

        file = fopen(LOKI_NETWORK_CACHE_PATH, "w");
        if (file == NULL) {
            LOG_WARN("Unable to write network cache: %s", strerror(errno));
            return;
        }

        fputs(app_state.known_networks, file);
        fclose(file);
    }
#endif
}

static void clear_known_network_cache(void)
{
    app_state.known_networks[0] = '\0';
    save_known_network_cache();
    if (app_state.config.standalone_mode) {
        set_discovery_message("NETWORK MEMORY CLEARED - SCOUT MODE RESET", app_state.config.discovery_hold_ms);
    } else {
        set_discovery_message("NETWORK MEMORY CLEARED", app_state.config.discovery_hold_ms);
    }
}

static void refresh_gps_state(void)
{
    const char *gps_fix = getenv("LOKI_GPS_FIX");
    float latitude;
    float longitude;
    float altitude_m;

    memset(&app_state.gps, 0, sizeof(app_state.gps));
    copy_text(app_state.gps.source, sizeof(app_state.gps.source), "awaiting module");

    if (gps_fix == NULL || gps_fix[0] == '\0') {
        return;
    }

    if (sscanf(gps_fix, " %f , %f , %f ", &latitude, &longitude, &altitude_m) == 3) {
        app_state.gps.has_fix = true;
        app_state.gps.latitude = latitude;
        app_state.gps.longitude = longitude;
        app_state.gps.altitude_m = altitude_m;
        copy_text(app_state.gps.source, sizeof(app_state.gps.source), "env:LOKI_GPS_FIX");
        return;
    }

    copy_text(app_state.gps.source, sizeof(app_state.gps.source), "invalid LOKI_GPS_FIX");
}

static void append_nearby_network(char *dest, size_t dest_size, const char *ssid, uint32_t *count)
{
    size_t dest_length;

    if (dest == NULL || dest_size == 0U || ssid == NULL || ssid[0] == '\0') {
        return;
    }

    if (network_list_contains(dest, ssid)) {
        return;
    }

    dest_length = strlen(dest);
    if (dest_length > 0U) {
        snprintf(dest + dest_length, dest_size - dest_length, " | %s", ssid);
    } else {
        copy_text(dest, dest_size, ssid);
    }

    if (count != NULL) {
        (*count)++;
    }
}

static void refresh_nearby_networks(void)
{
    char discoveries[LOKI_DISCOVERY_MESSAGE_MAX];
    bool cache_dirty = false;

    copy_text(app_state.nearby_networks, sizeof(app_state.nearby_networks), "scan unavailable");
    app_state.nearby_network_count = 0U;
    discoveries[0] = '\0';

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
    {
        FILE *pipe;
        char line[256];
        char command[160];
        char interface_name[32] = "";

        pipe = popen("iw dev 2>/dev/null", "r");
        if (pipe != NULL) {
            while (fgets(line, sizeof(line), pipe) != NULL) {
                char *match = strstr(line, "Interface ");
                if (match != NULL) {
                    copy_text(interface_name, sizeof(interface_name), match + 10);
                    interface_name[strcspn(interface_name, "\r\n\t ")] = '\0';
                    break;
                }
            }
            pclose(pipe);
        }

        if (interface_name[0] != '\0') {
            snprintf(command,
                     sizeof(command),
                     "iw dev %s scan ap-force 2>/dev/null",
                     interface_name);
            pipe = popen(command, "r");
            if (pipe != NULL) {
                app_state.nearby_networks[0] = '\0';
                while (fgets(line, sizeof(line), pipe) != NULL) {
                    char *match = strstr(line, "SSID: ");
                    if (match != NULL) {
                        char ssid[96];
                        copy_text(ssid, sizeof(ssid), match + 6);
                        ssid[strcspn(ssid, "\r\n")] = '\0';
                        if (ssid[0] != '\0') {
                            if (!network_list_contains(app_state.known_networks, ssid)) {
                                append_nearby_network(app_state.known_networks,
                                                      sizeof(app_state.known_networks),
                                                      ssid,
                                                      NULL);
                                cache_dirty = true;
                                append_nearby_network(discoveries,
                                                      sizeof(discoveries),
                                                      ssid,
                                                      NULL);
                            }
                            append_nearby_network(app_state.nearby_networks,
                                                  sizeof(app_state.nearby_networks),
                                                  ssid,
                                                  &app_state.nearby_network_count);
                            if (app_state.nearby_network_count >= 6U) {
                                break;
                            }
                        }
                    }
                }
                pclose(pipe);
            }
        }

        if (app_state.nearby_network_count == 0U) {
            copy_text(app_state.nearby_networks, sizeof(app_state.nearby_networks), "none detected");
        }
    }
#endif

    if (cache_dirty) {
        save_known_network_cache();
    }

    if (app_state.config.announce_new_networks && discoveries[0] != '\0') {
        char message[LOKI_DISCOVERY_MESSAGE_MAX];

        snprintf(message, sizeof(message), "NEW NETWORKS %s", discoveries);
        set_discovery_message(message, app_state.config.discovery_hold_ms);
    } else if (app_state.config.standalone_mode && app_state.discovery_hold_remaining_ms == 0U) {
        set_discovery_message("OFFLINE SCOUT MODE - WAITING FOR NEW NETWORKS", 0U);
    } else if (!app_state.config.standalone_mode && app_state.discovery_hold_remaining_ms == 0U) {
        app_state.discovery_message[0] = '\0';
    }
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

    copy_text(dest, dest_size, app_state.config.standalone_mode ? "standalone" : "offline");

    if (app_state.config.web.bind_address[0] != '\0' &&
        strcmp(app_state.config.web.bind_address, "0.0.0.0") != 0) {
        copy_text(dest, dest_size, app_state.config.web.bind_address);
    }

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
    {
        struct ifaddrs *ifaddr = NULL;
        struct ifaddrs *entry;
        char best_ip[32] = "";
        int best_score = -1;

        if (getifaddrs(&ifaddr) != 0) {
            struct addrinfo hints;
            struct addrinfo *result = NULL;
            char host[64];

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;

            if (gethostname(host, sizeof(host)) == 0 &&
                getaddrinfo(host, NULL, &hints, &result) == 0) {
                struct addrinfo *candidate;
                for (candidate = result; candidate != NULL; candidate = candidate->ai_next) {
                    struct sockaddr_in *address = (struct sockaddr_in *)candidate->ai_addr;
                    const unsigned char *bytes = (const unsigned char *)&address->sin_addr.s_addr;
                    char resolved_ip[32];

                    if (inet_ntop(AF_INET, &address->sin_addr, resolved_ip, sizeof(resolved_ip)) == NULL) {
                        continue;
                    }
                    if (strcmp(resolved_ip, "127.0.0.1") == 0 || strcmp(resolved_ip, "0.0.0.0") == 0) {
                        continue;
                    }
                    if (bytes[0] == 169U && bytes[1] == 254U) {
                        continue;
                    }

                    copy_text(dest, dest_size, resolved_ip);
                    break;
                }
                freeaddrinfo(result);
            }
            return;
        }

        for (entry = ifaddr; entry != NULL; entry = entry->ifa_next) {
            struct sockaddr_in *address;
            const unsigned char *bytes;
            char candidate_ip[32];
            int score = 0;

            if (entry->ifa_addr == NULL || entry->ifa_addr->sa_family != AF_INET) {
                continue;
            }
            if ((entry->ifa_flags & IFF_LOOPBACK) != 0 || (entry->ifa_flags & IFF_UP) == 0) {
                continue;
            }

            address = (struct sockaddr_in *)entry->ifa_addr;
            bytes = (const unsigned char *)&address->sin_addr.s_addr;
            if (inet_ntop(AF_INET, &address->sin_addr, candidate_ip, sizeof(candidate_ip)) == NULL) {
                continue;
            }
            if (strcmp(candidate_ip, "0.0.0.0") == 0) {
                continue;
            }
            if (bytes[0] == 169U && bytes[1] == 254U) {
                continue;
            }

            if (strncmp(entry->ifa_name, "wlan", 4) == 0 || strncmp(entry->ifa_name, "wl", 2) == 0) {
                score = 3;
            } else if (strncmp(entry->ifa_name, "eth", 3) == 0 || strncmp(entry->ifa_name, "en", 2) == 0) {
                score = 2;
            } else {
                score = 1;
            }

            if (score > best_score) {
                best_score = score;
                copy_text(best_ip, sizeof(best_ip), candidate_ip);
            }
        }

        freeifaddrs(ifaddr);

        if (best_ip[0] != '\0') {
            copy_text(dest, dest_size, best_ip);
        }
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

static void json_append_text(char *dest, size_t dest_size, const char *src)
{
    size_t current_length;
    size_t remaining;
    int written;

    if (dest == NULL || dest_size == 0U || src == NULL) {
        return;
    }

    current_length = strlen(dest);
    if (current_length >= dest_size - 1U) {
        return;
    }

    remaining = dest_size - current_length;
    written = snprintf(dest + current_length, remaining, "%s", src);
    (void)written;
}

static void json_append_format(char *dest, size_t dest_size, const char *format, ...)
{
    size_t current_length;
    size_t remaining;
    va_list args;
    int written;

    if (dest == NULL || dest_size == 0U || format == NULL) {
        return;
    }

    current_length = strlen(dest);
    if (current_length >= dest_size - 1U) {
        return;
    }

    remaining = dest_size - current_length;
    va_start(args, format);
    written = vsnprintf(dest + current_length, remaining, format, args);
    va_end(args);
    (void)written;
}

static void json_append_escaped(char *dest, size_t dest_size, const char *src)
{
    size_t index;

    if (src == NULL) {
        return;
    }

    for (index = 0; src[index] != '\0'; index++) {
        unsigned char character = (unsigned char)src[index];

        switch (character) {
            case '\\':
                json_append_text(dest, dest_size, "\\\\"");
                break;
            case '"':
                json_append_text(dest, dest_size, "\\\"");
                break;
            case '\n':
                json_append_text(dest, dest_size, "\\n");
                break;
            case '\r':
                json_append_text(dest, dest_size, "\\r");
                break;
            case '\t':
                json_append_text(dest, dest_size, "\\t");
                break;
            default:
                if (character < 32U) {
                    char escape[7];
                    snprintf(escape, sizeof(escape), "\\u%04x", character);
                    json_append_text(dest, dest_size, escape);
                } else {
                    char plain[2];
                    plain[0] = (char)character;
                    plain[1] = '\0';
                    json_append_text(dest, dest_size, plain);
                }
                break;
        }
    }
}

static const char *json_find_value_start(const char *json_body, const char *key)
{
    char pattern[64];
    const char *match;
    const char *cursor;

    if (json_body == NULL || key == NULL) {
        return NULL;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    match = strstr(json_body, pattern);
    if (match == NULL) {
        return NULL;
    }

    cursor = match + strlen(pattern);
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
        cursor++;
    }
    if (*cursor != ':') {
        return NULL;
    }
    cursor++;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
        cursor++;
    }

    return cursor;
}

static bool json_get_string_value(const char *json_body, const char *key, char *dest, size_t dest_size)
{
    const char *cursor = json_find_value_start(json_body, key);
    size_t write_index = 0U;

    if (dest == NULL || dest_size == 0U) {
        return false;
    }

    dest[0] = '\0';
    if (cursor == NULL || *cursor != '"') {
        return false;
    }

    cursor++;
    while (*cursor != '\0' && *cursor != '"' && write_index < dest_size - 1U) {
        if (*cursor == '\\' && cursor[1] != '\0') {
            cursor++;
            switch (*cursor) {
                case 'n': dest[write_index++] = '\n'; break;
                case 'r': dest[write_index++] = '\r'; break;
                case 't': dest[write_index++] = '\t'; break;
                case '\\': dest[write_index++] = '\\'; break;
                case '"': dest[write_index++] = '"'; break;
                default: dest[write_index++] = *cursor; break;
            }
            cursor++;
            continue;
        }

        dest[write_index++] = *cursor++;
    }

    dest[write_index] = '\0';
    return true;
}

static bool json_get_bool_value(const char *json_body, const char *key, bool *dest)
{
    const char *cursor = json_find_value_start(json_body, key);

    if (cursor == NULL || dest == NULL) {
        return false;
    }

    if (strncmp(cursor, "true", 4) == 0) {
        *dest = true;
        return true;
    }
    if (strncmp(cursor, "false", 5) == 0) {
        *dest = false;
        return true;
    }

    return false;
}

static bool json_get_uint_value(const char *json_body, const char *key, uint32_t *dest)
{
    const char *cursor = json_find_value_start(json_body, key);
    char *end_ptr;
    unsigned long value;

    if (cursor == NULL || dest == NULL) {
        return false;
    }

    value = strtoul(cursor, &end_ptr, 10);
    if (end_ptr == cursor) {
        return false;
    }

    *dest = (uint32_t)value;
    return true;
}

static bool json_get_float_value(const char *json_body, const char *key, float *dest)
{
    const char *cursor = json_find_value_start(json_body, key);
    char *end_ptr;
    float value;

    if (cursor == NULL || dest == NULL) {
        return false;
    }

    value = strtof(cursor, &end_ptr);
    if (end_ptr == cursor) {
        return false;
    }

    *dest = value;
    return true;
}

static void apply_extended_config_fields_from_json(loki_config_t *config, const char *json_body)
{
    uint32_t number_value;
    float float_value;
    bool bool_value;
    char text_value[LOKI_CONFIG_STRING_MAX];

    if (config == NULL || json_body == NULL) {
        return;
    }

    if (json_get_string_value(json_body, "profile", text_value, sizeof(text_value))) {
        copy_text(config->dragon.profile, sizeof(config->dragon.profile), text_value);
    }
    if (json_get_string_value(json_body, "temperament", text_value, sizeof(text_value))) {
        copy_text(config->dragon.temperament, sizeof(config->dragon.temperament), text_value);
    }
    if (json_get_bool_value(json_body, "plugin_display", &bool_value)) {
        config->plugins.display = bool_value;
    }
    if (json_get_bool_value(json_body, "plugin_dragon_ai", &bool_value)) {
        config->plugins.dragon_ai = bool_value;
    }
    if (json_get_bool_value(json_body, "plugin_network_state", &bool_value)) {
        config->plugins.network_state = bool_value;
    }
    if (json_get_bool_value(json_body, "standalone_mode", &bool_value)) {
        config->standalone_mode = bool_value;
    }
    if (json_get_bool_value(json_body, "announce_new_networks", &bool_value)) {
        config->announce_new_networks = bool_value;
    }
    if (json_get_float_value(json_body, "base_energy", &float_value)) {
        config->dragon.base_energy = float_value;
    }
    if (json_get_float_value(json_body, "base_curiosity", &float_value)) {
        config->dragon.base_curiosity = float_value;
    }
    if (json_get_float_value(json_body, "base_bond", &float_value)) {
        config->dragon.base_bond = float_value;
    }
    if (json_get_float_value(json_body, "base_hunger", &float_value)) {
        config->dragon.base_hunger = float_value;
    }
    if (json_get_float_value(json_body, "hunger_rate", &float_value)) {
        config->dragon.hunger_rate = float_value;
    }
    if (json_get_float_value(json_body, "energy_decay", &float_value)) {
        config->dragon.energy_decay = float_value;
    }
    if (json_get_float_value(json_body, "curiosity_rate", &float_value)) {
        config->dragon.curiosity_rate = float_value;
    }
    if (json_get_float_value(json_body, "explore_xp_gain", &float_value)) {
        config->dragon.explore_xp_gain = float_value;
    }
    if (json_get_float_value(json_body, "play_xp_gain", &float_value)) {
        config->dragon.play_xp_gain = float_value;
    }
    if (json_get_uint_value(json_body, "growth_interval_ms", &number_value)) {
        config->dragon.growth_interval_ms = number_value;
    }
    if (json_get_float_value(json_body, "growth_xp_step", &float_value)) {
        config->dragon.growth_xp_step = float_value;
    }
    if (json_get_uint_value(json_body, "egg_stage_max", &number_value)) {
        config->dragon.egg_stage_max = number_value;
    }
    if (json_get_uint_value(json_body, "hatchling_stage_max", &number_value)) {
        config->dragon.hatchling_stage_max = number_value;
    }
    if (json_get_uint_value(json_body, "discovery_hold_ms", &number_value)) {
        config->discovery_hold_ms = number_value;
    }
}

static void apply_extended_config_fields_from_form(loki_config_t *config, const char *form_data)
{
    char value[LOKI_TOOL_COMMAND_MAX];

    if (config == NULL || form_data == NULL) {
        return;
    }

    if (form_get_value(form_data, "profile", value, sizeof(value))) {
        copy_text(config->dragon.profile, sizeof(config->dragon.profile), value);
    }
    if (form_get_value(form_data, "temperament", value, sizeof(value))) {
        copy_text(config->dragon.temperament, sizeof(config->dragon.temperament), value);
    }
    config->plugins.display = form_get_value(form_data, "plugin_display", value, sizeof(value));
    config->plugins.dragon_ai = form_get_value(form_data, "plugin_dragon_ai", value, sizeof(value));
    config->plugins.network_state = form_get_value(form_data, "plugin_network_state", value, sizeof(value));
    config->standalone_mode = form_get_value(form_data, "standalone_mode", value, sizeof(value));
    config->announce_new_networks = form_get_value(form_data, "announce_new_networks", value, sizeof(value));
    if (form_get_value(form_data, "base_energy", value, sizeof(value))) {
        config->dragon.base_energy = strtof(value, NULL);
    }
    if (form_get_value(form_data, "base_curiosity", value, sizeof(value))) {
        config->dragon.base_curiosity = strtof(value, NULL);
    }
    if (form_get_value(form_data, "base_bond", value, sizeof(value))) {
        config->dragon.base_bond = strtof(value, NULL);
    }
    if (form_get_value(form_data, "base_hunger", value, sizeof(value))) {
        config->dragon.base_hunger = strtof(value, NULL);
    }
    if (form_get_value(form_data, "hunger_rate", value, sizeof(value))) {
        config->dragon.hunger_rate = strtof(value, NULL);
    }
    if (form_get_value(form_data, "energy_decay", value, sizeof(value))) {
        config->dragon.energy_decay = strtof(value, NULL);
    }
    if (form_get_value(form_data, "curiosity_rate", value, sizeof(value))) {
        config->dragon.curiosity_rate = strtof(value, NULL);
    }
    if (form_get_value(form_data, "explore_xp_gain", value, sizeof(value))) {
        config->dragon.explore_xp_gain = strtof(value, NULL);
    }
    if (form_get_value(form_data, "play_xp_gain", value, sizeof(value))) {
        config->dragon.play_xp_gain = strtof(value, NULL);
    }
    if (form_get_value(form_data, "growth_interval_ms", value, sizeof(value))) {
        config->dragon.growth_interval_ms = (uint32_t)strtoul(value, NULL, 10);
    }
    if (form_get_value(form_data, "growth_xp_step", value, sizeof(value))) {
        config->dragon.growth_xp_step = strtof(value, NULL);
    }
    if (form_get_value(form_data, "egg_stage_max", value, sizeof(value))) {
        config->dragon.egg_stage_max = (uint32_t)strtoul(value, NULL, 10);
    }
    if (form_get_value(form_data, "hatchling_stage_max", value, sizeof(value))) {
        config->dragon.hatchling_stage_max = (uint32_t)strtoul(value, NULL, 10);
    }
    if (form_get_value(form_data, "discovery_hold_ms", value, sizeof(value))) {
        config->discovery_hold_ms = (uint32_t)strtoul(value, NULL, 10);
    }
}

static bool path_get_query_value(const char *path, const char *key, char *dest, size_t dest_size)
{
    const char *query;
    size_t key_length;

    if (path == NULL || key == NULL || dest == NULL || dest_size == 0U) {
        return false;
    }

    dest[0] = '\0';
    query = strchr(path, '?');
    if (query == NULL) {
        return false;
    }

    query++;
    key_length = strlen(key);
    while (*query != '\0') {
        const char *pair_end = strchr(query, '&');
        const char *equals = strchr(query, '=');
        size_t key_size;
        size_t value_size;

        if (pair_end == NULL) {
            pair_end = query + strlen(query);
        }
        if (equals != NULL && equals < pair_end) {
            key_size = (size_t)(equals - query);
            if (key_size == key_length && strncmp(query, key, key_length) == 0) {
                value_size = (size_t)(pair_end - equals - 1);
                if (value_size >= dest_size) {
                    value_size = dest_size - 1U;
                }
                memcpy(dest, equals + 1, value_size);
                dest[value_size] = '\0';
                return true;
            }
        }

        if (*pair_end == '\0') {
            break;
        }
        query = pair_end + 1;
    }

    return false;
}

static hal_status_t apply_updated_config(app_state_t *state,
                                         loki_config_t *updated,
                                         char *message,
                                         size_t message_size)
{
    if (state == NULL || updated == NULL || message == NULL || message_size == 0U) {
        return HAL_INVALID_PARAM;
    }

    if (updated->scan_interval_ms == 0U) {
        updated->scan_interval_ms = 15000U;
    }
    if (updated->discovery_hold_ms == 0U) {
        updated->discovery_hold_ms = 20000U;
    }
    if (updated->web.port == 0U) {
        updated->web.port = 8080U;
    }
    if (updated->ai.actor_learning_rate <= 0.0f) {
        updated->ai.actor_learning_rate = updated->ai.learning_rate;
    }
    if (updated->ai.critic_learning_rate <= 0.0f) {
        updated->ai.critic_learning_rate = updated->ai.learning_rate;
    }
    if (updated->ai.policy_temperature <= 0.05f) {
        updated->ai.policy_temperature = 0.05f;
    }

    loki_config_cleanup(&state->config);
    state->config = *updated;
    updated->tools = NULL;
    updated->tool_count = 0U;
    updated->tool_capacity = 0U;

#if PLATFORM == PLATFORM_LINUX
    setenv("LOKI_DISPLAY_BACKEND", state->config.display.backend, 1);
    setenv("LOKI_FB_DEVICE", state->config.display.framebuffer_device, 1);
#endif

    if (loki_config_save(LOKI_CONFIG_PATH, &state->config) != HAL_OK) {
        snprintf(message, message_size, "Failed to save loki.conf.");
        return HAL_ERROR;
    }

    snprintf(message, message_size, "Saved loki.conf. Some display and web port changes take effect after restart.");
    return HAL_OK;
}

static void append_tool_json(char *output, size_t output_size, size_t tool_id, const loki_tool_config_t *tool)
{
    json_append_format(output, output_size, "{\"id\":%u,\"section\":\"", (unsigned int)tool_id);
    json_append_escaped(output, output_size, tool->section);
    json_append_text(output, output_size, "\",\"enabled\":");
    json_append_text(output, output_size, tool->enabled ? "true" : "false");
    json_append_text(output, output_size, ",\"name\":\"");
    json_append_escaped(output, output_size, tool->name);
    json_append_text(output, output_size, "\",\"description\":\"");
    json_append_escaped(output, output_size, tool->description);
    json_append_text(output, output_size, "\",\"command\":\"");
    json_append_escaped(output, output_size, tool->command);
    json_append_text(output, output_size, "\"}");
}

static void build_tools_json(const app_state_t *state, const char *path, char *output, size_t output_size)
{
    char id_text[16];
    size_t index;

    output[0] = '\0';
    if (path_get_query_value(path, "id", id_text, sizeof(id_text))) {
        size_t tool_id = (size_t)strtoul(id_text, NULL, 10);
        if (tool_id == 0U || tool_id > state->config.tool_count) {
            json_append_text(output, output_size, "{\"error\":\"tool not found\"}");
            return;
        }
        json_append_text(output, output_size, "{\"tool\":");
        append_tool_json(output, output_size, tool_id, &state->config.tools[tool_id - 1U]);
        json_append_text(output, output_size, "}");
        return;
    }

    json_append_format(output, output_size, "{\"count\":%u,\"tools\":[", (unsigned int)state->config.tool_count);
    for (index = 0; index < state->config.tool_count; index++) {
        if (index > 0U) {
            json_append_text(output, output_size, ",");
        }
        append_tool_json(output, output_size, index + 1U, &state->config.tools[index]);
    }
    json_append_text(output, output_size, "]}");
}

static void build_capabilities_json(const app_state_t *state, char *output, size_t output_size)
{
    (void)state;

    output[0] = '\0';
    json_append_text(output,
                     output_size,
                     "{\"api\":[\"GET /api/status\",\"GET /api/tools\",\"POST /api/tools\",\"POST /api/tools/run?id=<id>\",\"PATCH /api/settings\",\"POST /api/networks/reset\",\"GET /api/capabilities\"],"
                     "\"tool_schemes\":[\"internal://status\",\"internal://loot\",\"internal://wardrive\",\"internal://gps\",\"internal://peripherals\",\"internal://usb\",\"internal://i2c\",\"internal://uart\",\"internal://gpio\",\"internal://nmap?target=<host>&mode=<ping|quick|service>\",\"internal://custom_scan?target=<host>&profile=<quick|service|udp-top>\",\"<raw shell command>\"],"
                     "\"security\":{\"runner\":\"raw tool commands execute as provided; protect this interface according to your deployment\"}}");
}

static hal_status_t api_reset_network_memory(app_state_t *state,
                                             char *response,
                                             size_t response_size)
{
    if (state == NULL || response == NULL || response_size == 0U) {
        return HAL_INVALID_PARAM;
    }

    clear_known_network_cache();
    refresh_nearby_networks();
    dragon_ai_apply_reward(&state->dragon, 0.05f);
    dragon_ai_award_xp(&state->dragon, 0.08f);

    response[0] = '\0';
    json_append_text(response, response_size, "{\"ok\":true,\"message\":\"Network memory cleared\",\"known_network_total\":0}");
    return HAL_OK;
}

static hal_status_t api_upsert_tool(app_state_t *state,
                                    const char *json_body,
                                    char *response,
                                    size_t response_size)
{
    loki_config_t updated;
    loki_tool_config_t *tool;
    char section[LOKI_CONFIG_STRING_MAX];
    char section_input[LOKI_CONFIG_STRING_MAX];
    char name[LOKI_CONFIG_STRING_MAX];
    char description[LOKI_TOOL_DESCRIPTION_MAX];
    char command[LOKI_TOOL_COMMAND_MAX];
    char message[256];
    size_t tool_id = 0U;
    bool enabled;
    bool has_enabled;
    hal_status_t status;

    if (state == NULL || json_body == NULL || response == NULL || response_size == 0U) {
        return HAL_INVALID_PARAM;
    }

    section_input[0] = '\0';
    name[0] = '\0';
    description[0] = '\0';
    command[0] = '\0';
    json_get_string_value(json_body, "section", section_input, sizeof(section_input));
    json_get_string_value(json_body, "name", name, sizeof(name));
    json_get_string_value(json_body, "description", description, sizeof(description));
    json_get_string_value(json_body, "command", command, sizeof(command));
    has_enabled = json_get_bool_value(json_body, "enabled", &enabled);

    if (section_input[0] == '\0' && name[0] == '\0') {
        snprintf(response, response_size, "{\"error\":\"section or name is required\"}");
        return HAL_INVALID_PARAM;
    }

    if (loki_config_clone(&updated, &state->config) != HAL_OK) {
        snprintf(response, response_size, "{\"error\":\"failed to prepare configuration update\"}");
        return HAL_ERROR;
    }

    build_tool_section_name(section, sizeof(section), section_input, name);
    if (loki_config_ensure_tool(&updated, section, &tool) != HAL_OK || tool == NULL) {
        loki_config_cleanup(&updated);
        snprintf(response, response_size, "{\"error\":\"failed to allocate tool slot\"}");
        return HAL_ERROR;
    }

    if (name[0] != '\0') {
        copy_text(tool->name, sizeof(tool->name), name);
    }
    if (description[0] != '\0') {
        copy_text(tool->description, sizeof(tool->description), description);
    }
    if (command[0] != '\0') {
        copy_text(tool->command, sizeof(tool->command), command);
    }
    if (has_enabled) {
        tool->enabled = enabled;
    }

    if (tool->name[0] == '\0' || tool->command[0] == '\0') {
        loki_config_cleanup(&updated);
        snprintf(response, response_size, "{\"error\":\"tool requires both name and command\"}");
        return HAL_INVALID_PARAM;
    }

    status = apply_updated_config(state, &updated, message, sizeof(message));
    if (status != HAL_OK) {
        loki_config_cleanup(&updated);
        snprintf(response, response_size, "{\"error\":\"");
        json_append_escaped(response, response_size, message);
        json_append_text(response, response_size, "\"}");
        return status;
    }

    dragon_ai_apply_reward(&state->dragon, 0.08f);
    dragon_ai_award_xp(&state->dragon, 0.10f);
    for (tool_id = 0U; tool_id < state->config.tool_count; tool_id++) {
        if (strcmp(state->config.tools[tool_id].section, section) == 0) {
            tool = &state->config.tools[tool_id];
            tool_id++;
            break;
        }
    }
    response[0] = '\0';
    json_append_text(response, response_size, "{\"ok\":true,\"message\":\"");
    json_append_escaped(response, response_size, message);
    json_append_text(response, response_size, "\",\"tool\":");
    append_tool_json(response, response_size, tool_id, tool);
    json_append_text(response, response_size, "}");
    return HAL_OK;
}

static hal_status_t api_run_tool_request(app_state_t *state,
                                         const char *path,
                                         bool request_is_localhost,
                                         const char *remote_address,
                                         char *response,
                                         size_t response_size)
{
    char id_text[16];
    char output[4096];
    size_t tool_id;
    hal_status_t status;

    if (state == NULL || response == NULL || response_size == 0U) {
        return HAL_INVALID_PARAM;
    }

    if (!path_get_query_value(path, "id", id_text, sizeof(id_text))) {
        snprintf(response, response_size, "{\"error\":\"missing tool id\"}");
        return HAL_INVALID_PARAM;
    }

    tool_id = (size_t)strtoul(id_text, NULL, 10);
    if (tool_id == 0U || tool_id > state->config.tool_count) {
        snprintf(response, response_size, "{\"error\":\"tool not found\"}");
        return HAL_INVALID_PARAM;
    }

    status = run_tool_from_slot(tool_id - 1U,
                                request_is_localhost,
                                remote_address,
                                output,
                                sizeof(output),
                                state);
    response[0] = '\0';
    json_append_text(response, response_size, "{\"ok\":");
    json_append_text(response, response_size, (status == HAL_OK) ? "true" : "false");
    json_append_format(response, response_size, ",\"id\":%u,\"name\":\"", (unsigned int)tool_id);
    json_append_escaped(response, response_size, state->config.tools[tool_id - 1U].name);
    json_append_text(response, response_size, "\",\"output\":\"");
    json_append_escaped(response, response_size, output);
    json_append_text(response, response_size, "\"}");
    return status;
}

static hal_status_t api_patch_settings(app_state_t *state,
                                       const char *json_body,
                                       char *response,
                                       size_t response_size)
{
    loki_config_t updated;
    char message[256];
    uint32_t number_value;
    float float_value;
    bool bool_value;
    char text_value[LOKI_CONFIG_STRING_MAX];
    hal_status_t status;

    if (state == NULL || json_body == NULL || response == NULL || response_size == 0U) {
        return HAL_INVALID_PARAM;
    }

    if (loki_config_clone(&updated, &state->config) != HAL_OK) {
        snprintf(response, response_size, "{\"error\":\"failed to prepare configuration update\"}");
        return HAL_ERROR;
    }

    if (json_get_string_value(json_body, "dragon_name", text_value, sizeof(text_value))) {
        copy_text(updated.dragon_name, sizeof(updated.dragon_name), text_value);
        copy_text(updated.device_name, sizeof(updated.device_name), text_value);
    }
    if (json_get_string_value(json_body, "device_name", text_value, sizeof(text_value))) {
        copy_text(updated.device_name, sizeof(updated.device_name), text_value);
    }
    if (json_get_string_value(json_body, "hostname", text_value, sizeof(text_value))) {
        copy_text(updated.hostname, sizeof(updated.hostname), text_value);
    }
    if (json_get_string_value(json_body, "preferred_ssid", text_value, sizeof(text_value))) {
        copy_text(updated.preferred_ssid, sizeof(updated.preferred_ssid), text_value);
    }
    if (json_get_string_value(json_body, "web_bind_address", text_value, sizeof(text_value))) {
        copy_text(updated.web.bind_address, sizeof(updated.web.bind_address), text_value);
    }
    if (json_get_string_value(json_body, "display_backend", text_value, sizeof(text_value))) {
        copy_text(updated.display.backend, sizeof(updated.display.backend), text_value);
    }
    if (json_get_string_value(json_body, "framebuffer_device", text_value, sizeof(text_value))) {
        copy_text(updated.display.framebuffer_device, sizeof(updated.display.framebuffer_device), text_value);
    }
    if (json_get_bool_value(json_body, "dhcp_enabled", &bool_value)) {
        updated.dhcp_enabled = bool_value;
    }
    if (json_get_bool_value(json_body, "web_ui_enabled", &bool_value)) {
        updated.web_ui_enabled = bool_value;
    }
    if (json_get_uint_value(json_body, "scan_interval_ms", &number_value)) {
        updated.scan_interval_ms = number_value;
    }
    if (json_get_uint_value(json_body, "hop_interval_ms", &number_value)) {
        updated.hop_interval_ms = number_value;
    }
    if (json_get_uint_value(json_body, "web_port", &number_value)) {
        updated.web.port = (uint16_t)number_value;
    }
    if (json_get_uint_value(json_body, "tick_interval_ms", &number_value)) {
        updated.ai.tick_interval_ms = number_value;
    }
    if (json_get_float_value(json_body, "learning_rate", &float_value)) {
        updated.ai.learning_rate = float_value;
    }
    if (json_get_float_value(json_body, "actor_learning_rate", &float_value)) {
        updated.ai.actor_learning_rate = float_value;
    }
    if (json_get_float_value(json_body, "critic_learning_rate", &float_value)) {
        updated.ai.critic_learning_rate = float_value;
    }
    if (json_get_float_value(json_body, "discount_factor", &float_value)) {
        updated.ai.discount_factor = float_value;
    }
    if (json_get_float_value(json_body, "reward_decay", &float_value)) {
        updated.ai.reward_decay = float_value;
    }
    if (json_get_float_value(json_body, "entropy_beta", &float_value)) {
        updated.ai.entropy_beta = float_value;
    }
    if (json_get_float_value(json_body, "policy_temperature", &float_value)) {
        updated.ai.policy_temperature = float_value;
    }

    apply_extended_config_fields_from_json(&updated, json_body);

    status = apply_updated_config(state, &updated, message, sizeof(message));
    if (status != HAL_OK) {
        loki_config_cleanup(&updated);
        snprintf(response, response_size, "{\"error\":\"");
        json_append_escaped(response, response_size, message);
        json_append_text(response, response_size, "\"}");
        return status;
    }

    dragon_ai_apply_reward(&state->dragon, 0.08f);
    response[0] = '\0';
    json_append_text(response, response_size, "{\"ok\":true,\"message\":\"");
    json_append_escaped(response, response_size, message);
    json_append_text(response, response_size, "\"}");
    return HAL_OK;
}

static hal_status_t handle_api_request(const char *method,
                                       const char *path,
                                       const char *body,
                                       bool request_is_localhost,
                                       const char *remote_address,
                                       char *response,
                                       size_t response_size,
                                       void *context)
{
    app_state_t *state = (app_state_t *)context;

    if (method == NULL || path == NULL || response == NULL || response_size == 0U || state == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (strcmp(method, "GET") == 0 && strncmp(path, "/api/tools", 10) == 0) {
        build_tools_json(state, path, response, response_size);
        return (strstr(response, "\"error\"") != NULL) ? HAL_INVALID_PARAM : HAL_OK;
    }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/credits") == 0) {
        snprintf(response, response_size, "{\"credits\":%.2f}", (double)state->credits_balance);
        return HAL_OK;
    }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/capabilities") == 0) {
        build_capabilities_json(state, response, response_size);
        return HAL_OK;
    }
    if (strcmp(method, "POST") == 0 && strncmp(path, "/api/tools/run", 14) == 0) {
        return api_run_tool_request(state,
                                    path,
                                    request_is_localhost,
                                    remote_address,
                                    response,
                                    response_size);
    }
    if ((strcmp(method, "POST") == 0 || strcmp(method, "PATCH") == 0) && strcmp(path, "/api/credits") == 0) {
        float new_value = state->credits_balance;
        hal_status_t status = HAL_OK;

        if (body == NULL || (!json_get_float_value(body, "credits", &new_value) &&
                              !json_get_float_value(body, "value", &new_value))) {
            snprintf(response, response_size, "{\"error\":\"missing credits/value field\"}");
            return HAL_INVALID_PARAM;
        }

        status = loki_eeprom_save_credits(new_value);
        if (status == HAL_OK) {
            state->credits_balance = new_value;
            snprintf(response, response_size, "{\"ok\":true,\"credits\":%.2f}", (double)new_value);
        } else {
            snprintf(response, response_size, "{\"error\":\"failed to persist credits to EEPROM\"}");
        }

        return status;
    }
    if ((strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) && strcmp(path, "/api/tools") == 0) {
        return api_upsert_tool(state, body != NULL ? body : "", response, response_size);
    }
    if ((strcmp(method, "PATCH") == 0 || strcmp(method, "POST") == 0) && strcmp(path, "/api/settings") == 0) {
        return api_patch_settings(state, body != NULL ? body : "", response, response_size);
    }
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/networks/reset") == 0) {
        return api_reset_network_memory(state, response, response_size);
    }

    snprintf(response, response_size, "{\"error\":\"endpoint not found\"}");
    return HAL_NOT_SUPPORTED;
}

static hal_status_t save_settings_from_form(const char *form_data,
                                           bool request_is_localhost,
                                           const char *remote_address,
                                           char *message,
                                           size_t message_size,
                                           void *context)
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

    (void)request_is_localhost;
    (void)remote_address;

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

    apply_extended_config_fields_from_form(&updated, form_data);

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

    if (apply_updated_config(state, &updated, message, message_size) != HAL_OK) {
        loki_config_cleanup(&updated);
        return HAL_ERROR;
    }

    dragon_ai_apply_reward(&state->dragon, 0.08f);
    dragon_ai_award_xp(&state->dragon, 0.05f);
    return HAL_OK;
}

static hal_status_t run_tool_from_slot(size_t tool_index,
                                       bool request_is_localhost,
                                       const char *remote_address,
                                       char *output,
                                       size_t output_size,
                                       void *context)
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

    refresh_gps_state();

    tool_context.config = &state->config;
    tool_context.dragon = dragon;
    tool_context.gps = &state->gps;
    tool_context.nearby_networks = state->nearby_networks;
    tool_context.request_remote_address = remote_address;
    tool_context.nearby_network_count = state->nearby_network_count;
    tool_context.heartbeat_ticks = state->heartbeat_ticks;
    tool_context.network_phase_ms = state->network_phase_ms;
    tool_context.request_is_localhost = request_is_localhost;

    if (state->running_tool_count < UINT32_MAX) {
        state->running_tool_count++;
    }

    status = loki_tool_run(&state->config.tools[tool_index], &tool_context, output, output_size);

    if (state->running_tool_count > 0U) {
        state->running_tool_count--;
    }
    if (status == HAL_OK) {
        dragon_ai_apply_reward(&state->dragon, 0.12f);
        dragon_ai_award_xp(&state->dragon, 0.30f);
    } else {
        dragon_ai_apply_reward(&state->dragon, -0.04f);
        dragon_ai_award_xp(&state->dragon, -0.05f);
    }

    return status;
}

static int16_t triangle_wave(uint32_t tick, uint32_t period, int16_t amplitude)
{
    uint32_t half_period;
    uint32_t phase;
    int32_t scaled;

    if (period < 2U || amplitude == 0) {
        return 0;
    }

    half_period = period / 2U;
    if (half_period == 0U) {
        return 0;
    }

    phase = tick % period;
    if (phase < half_period) {
        scaled = ((int32_t)phase * (int32_t)(amplitude * 2)) / (int32_t)half_period;
        return (int16_t)(scaled - amplitude);
    }

    scaled = ((int32_t)(phase - half_period) * (int32_t)(amplitude * 2)) / (int32_t)half_period;
    return (int16_t)(amplitude - scaled);
}

static uint16_t positive_wave(uint32_t tick, uint32_t period, uint16_t amplitude)
{
    int16_t wave = triangle_wave(tick, period, (int16_t)amplitude);
    return (uint16_t)((wave < 0) ? -wave : wave);
}

static void draw_text_line_offset(int16_t x, uint16_t y, const char *text, color_t color, uint8_t scale)
{
    int16_t cursor_x = x;
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
                int16_t draw_x = (int16_t)(cursor_x + (int16_t)(column * scale));

                if ((glyph[row] & (1U << (4U - column))) == 0U) {
                    continue;
                }
                if (draw_x < 0 || draw_x >= (int16_t)TFT_WIDTH) {
                    continue;
                }

                tft_fill_rect((uint16_t)draw_x,
                              (uint16_t)(y + (row * scale)),
                              scale,
                              scale,
                              color);
            }
        }

        cursor_x = (int16_t)(cursor_x + (int16_t)(6U * scale));
    }
}

static void draw_discovery_ticker(uint32_t tick, color_t accent)
{
    bool alert_active;
    uint8_t scale;
    size_t text_length;
    int16_t text_width;
    int16_t offset;
    uint16_t band_height;
    uint16_t pulse;
    uint16_t glow_width;
    color_t banner_color;
    color_t text_color;
    color_t flash_color;
    const char *label;
    const char *message = app_state.discovery_message;

    if (message == NULL || message[0] == '\0') {
        return;
    }

    alert_active = (app_state.discovery_hold_remaining_ms > 0U);
    scale = alert_active ? 2U : 1U;
    band_height = alert_active ? 34U : 22U;
    pulse = positive_wave(tick + 3U, 10U, alert_active ? 28U : 10U);
    glow_width = (uint16_t)(36U + positive_wave(tick + 7U, 16U, 120U));
    banner_color = alert_active ? RGB565(72, 10, 10) : RGB565(8, 16, 24);
    text_color = alert_active ? RGB565(255, 238, 214) : RGB565(206, 243, 236);
    flash_color = alert_active ? RGB565(255, 146, 96) : accent;
    label = alert_active ? "ALERT" : "SCOUT";

    tft_fill_rect(0, (uint16_t)(TFT_HEIGHT - band_height), TFT_WIDTH, band_height, banner_color);
    tft_fill_rect(0, (uint16_t)(TFT_HEIGHT - band_height), TFT_WIDTH, 3, accent);
    tft_fill_rect(0, (uint16_t)(TFT_HEIGHT - 3U), TFT_WIDTH, 3, alert_active ? RGB565(255, 120, 88) : RGB565(24, 56, 72));
    tft_fill_rect(0, (uint16_t)(TFT_HEIGHT - band_height), 84, band_height, alert_active ? RGB565(122, 18, 16) : RGB565(10, 34, 42));
    tft_fill_rect(92, (uint16_t)(TFT_HEIGHT - band_height + 4U), glow_width, 4, accent);
    tft_fill_rect((uint16_t)(TFT_WIDTH - 48U - pulse), (uint16_t)(TFT_HEIGHT - band_height + 6U), (uint16_t)(16U + pulse), 5, alert_active ? RGB565(255, 208, 104) : accent);
    if (alert_active) {
        uint16_t flash_left = (uint16_t)((tick * 17U) % TFT_WIDTH);
        uint16_t flash_width = (uint16_t)(48U + positive_wave(tick + 5U, 8U, 64U));

        tft_fill_rect(0, 0, TFT_WIDTH, 6, flash_color);
        tft_fill_rect(0, 12, TFT_WIDTH, 2, RGB565(92, 18, 18));
        tft_fill_rect(flash_left, (uint16_t)(TFT_HEIGHT - band_height), flash_width, band_height, RGB565(110, 24, 16));
        tft_fill_rect((uint16_t)((flash_left + 28U) % TFT_WIDTH), (uint16_t)(TFT_HEIGHT - band_height + 4U), (uint16_t)(flash_width / 2U), 6, RGB565(255, 214, 132));
    }
    draw_text_line(12, (uint16_t)(TFT_HEIGHT - band_height + (alert_active ? 10U : 8U)), label, alert_active ? RGB565(255, 236, 194) : accent, alert_active ? 2U : 1U);

    text_length = strlen(message);
    text_width = (int16_t)(text_length * 6U * scale);
    if (text_width <= 0) {
        return;
    }

    offset = (int16_t)(TFT_WIDTH - ((tick * (alert_active ? 4U : 2U)) % (uint32_t)(text_width + TFT_WIDTH + 48U)));
    draw_text_line_offset(offset, (uint16_t)(TFT_HEIGHT - band_height + (alert_active ? 10U : 8U)), message, text_color, scale);
    if ((offset + text_width) < (int16_t)TFT_WIDTH) {
        draw_text_line_offset((int16_t)(offset + text_width + 48),
                              (uint16_t)(TFT_HEIGHT - band_height + (alert_active ? 10U : 8U)),
                              message,
                              alert_active ? RGB565(255, 190, 110) : accent,
                              scale);
    }
}

static void draw_dashboard_ambience(uint32_t tick, color_t accent)
{
    uint16_t pulse_x = (uint16_t)(126U + ((tick * 3U) % 128U));
    uint16_t ember_x = (uint16_t)(332U + (tick * 5U) % 86U);

    /* Keep ambience away from telemetry text blocks so GPS/stats stay readable. */
    tft_fill_rect(110, 36, 140, 1, RGB565(24, 42, 62));
    tft_fill_rect(110, 198, 140, 1, RGB565(24, 42, 62));
    tft_fill_rect(112, 36, 68, 1, accent);
    tft_fill_rect(pulse_x, 196, 14, 2, RGB565(44, 90, 124));

        // Removed background overlays for clarity
    tft_fill_rect(302, 44, 122, 2, RGB565(28, 42, 64));
    tft_fill_rect(302, 46, 78, 1, accent);

    tft_fill_rect(ember_x, 268, 14, 2, accent);
    tft_fill_rect((uint16_t)(ember_x + 4U), 264, 5, 2, RGB565(255, 238, 214));
}

static void draw_horizontal_span(int16_t x, int16_t y, int16_t width, color_t color)
{
    int16_t clipped_x = x;
    int16_t clipped_width = width;

    if (y < 0 || y >= (int16_t)TFT_HEIGHT || width <= 0) {
        return;
    }

    if (clipped_x < 0) {
        // Removed animated overlays for clarity
        clipped_width = (int16_t)(clipped_width + clipped_x);
        clipped_x = 0;
    }
    if (clipped_x >= (int16_t)TFT_WIDTH) {
        return;
    }
    if ((clipped_x + clipped_width) > (int16_t)TFT_WIDTH) {
        clipped_width = (int16_t)((int16_t)TFT_WIDTH - clipped_x);
    }
    if (clipped_width <= 0) {
        return;
    }

    tft_fill_rect((uint16_t)clipped_x, (uint16_t)y, (uint16_t)clipped_width, 1U, color);
}

static void draw_horizontal_stack(int16_t x,
                                  int16_t y,
                                  int16_t width,
                                  uint8_t thickness,
                                  color_t color)
{
    uint8_t line;

    if (thickness == 0U) {
        return;
    }

    for (line = 0U; line < thickness; line++) {
        draw_horizontal_span(x, (int16_t)(y + line), width, color);
    }
}

static void draw_diagonal_stroke(int16_t x,
                                 int16_t y,
                                 int16_t height,
                                 int16_t drift,
                                 uint8_t thickness,
                                 color_t color)
{
    int16_t row;

    if (height <= 0 || thickness == 0U) {
        return;
    }

    for (row = 0; row < height; row++) {
        int16_t offset = (int16_t)((row * drift) / height);

        draw_horizontal_stack((int16_t)(x + offset),
                              (int16_t)(y + row),
                              (int16_t)thickness,
                              1U,
                              color);
    }
}

static void draw_soft_ellipse(int16_t center_x,
                              int16_t center_y,
                              int16_t radius_x,
                              int16_t radius_y,
                              color_t color)
{
    int16_t row;

    if (radius_x <= 0 || radius_y <= 0) {
        return;
    }

    for (row = -radius_y; row <= radius_y; row++) {
        float normalized = (float)row / (float)radius_y;
        float inside = 1.0f - (normalized * normalized);
        int16_t half_width;

        if (inside <= 0.0f) {
            half_width = 0;
        } else {
            half_width = (int16_t)((float)radius_x * sqrtf(inside));
        }

        draw_horizontal_span((int16_t)(center_x - half_width),
                             (int16_t)(center_y + row),
                             (int16_t)((half_width * 2) + 1),
                             color);
    }
}

static void draw_tapered_wing(int16_t root_x,
                              int16_t root_y,
                              int16_t width,
                              int16_t height,
                              int16_t drift,
                              color_t color,
                              bool mirror)
{
    int16_t row;

    if (width <= 0 || height <= 0) {
        return;
    }

    for (row = 0; row < height; row++) {
        int16_t span = (int16_t)(width - ((row * width) / height));
        int16_t shift = (int16_t)((row * drift) / height);
        int16_t left = mirror ? (int16_t)(root_x - shift - span) : (int16_t)(root_x + shift);

        draw_horizontal_span(left, (int16_t)(root_y + row), span, color);
    }
}

static void draw_outlined_ellipse(int16_t center_x,
                                  int16_t center_y,
                                  int16_t radius_x,
                                  int16_t radius_y,
                                  int16_t stroke,
                                  color_t fill,
                                  color_t outline)
{
    if (radius_x <= 0 || radius_y <= 0) {
        return;
    }

    if (stroke < 1) {
        stroke = 1;
    }

    draw_soft_ellipse(center_x, center_y, radius_x, radius_y, outline);
    if (radius_x > stroke && radius_y > stroke) {
        draw_soft_ellipse(center_x,
                          center_y,
                          (int16_t)(radius_x - stroke),
                          (int16_t)(radius_y - stroke),
                          fill);
    }
}

static void draw_outlined_wing(int16_t root_x,
                               int16_t root_y,
                               int16_t width,
                               int16_t height,
                               int16_t drift,
                               bool mirror,
                               color_t membrane,
                               color_t outline)
{
    int16_t inset = 5;

    draw_tapered_wing(root_x, root_y, width, height, drift, outline, mirror);
    if (width > (inset * 2) && height > inset) {
        int16_t inner_root = mirror ? (int16_t)(root_x - inset) : (int16_t)(root_x + inset);
        draw_tapered_wing(inner_root,
                          (int16_t)(root_y + inset),
                          (int16_t)(width - (inset * 2)),
                          (int16_t)(height - inset),
                          (int16_t)(drift > 0 ? drift - 2 : 0),
                          membrane,
                          mirror);
    }
}

static void draw_wing_bones(int16_t root_x,
                            int16_t root_y,
                            int16_t width,
                            int16_t height,
                            bool mirror,
                            color_t color)
{
    int16_t drift_primary = mirror ? (int16_t)(-(width / 3)) : (int16_t)(width / 3);
    int16_t drift_secondary = mirror ? (int16_t)(-(width / 2)) : (int16_t)(width / 2);
    int16_t drift_outer = mirror ? (int16_t)(-((width * 2) / 3)) : (int16_t)((width * 2) / 3);

    draw_diagonal_stroke(root_x, (int16_t)(root_y + 6), (int16_t)(height - 10), drift_primary, 2U, color);
    draw_diagonal_stroke((int16_t)(root_x + (mirror ? -2 : 2)),
                         (int16_t)(root_y + 10),
                         (int16_t)(height - 18),
                         drift_secondary,
                         2U,
                         color);
    draw_diagonal_stroke((int16_t)(root_x + (mirror ? -4 : 4)),
                         (int16_t)(root_y + 14),
                         (int16_t)(height - 28),
                         drift_outer,
                         1U,
                         color);
}

static void draw_cartoon_eye(int16_t x,
                             int16_t y,
                             bool blink,
                             color_t outline,
                             color_t sclera,
                             color_t iris)
{
    if (blink) {
        draw_horizontal_span((int16_t)(x - 6), y, 12, sclera);
        draw_horizontal_span((int16_t)(x - 5), (int16_t)(y + 1), 10, outline);
        return;
    }

    draw_outlined_ellipse(x, y, 6, 5, 1, sclera, outline);
    draw_soft_ellipse((int16_t)(x + 1), (int16_t)(y + 1), 3, 3, iris);
    draw_soft_ellipse((int16_t)(x + 2), (int16_t)(y + 1), 1, 1, COLOR_BLACK);
    draw_soft_ellipse((int16_t)(x - 1), (int16_t)(y - 1), 1, 1, COLOR_WHITE);
}

static void draw_dragon_shadow(int16_t center_x, int16_t center_y, int16_t radius_x, int16_t radius_y)
{
    draw_soft_ellipse(center_x, center_y, radius_x, radius_y, RGB565(4, 8, 14));
    if (radius_x > 8 && radius_y > 2) {
        draw_soft_ellipse(center_x, (int16_t)(center_y - 1), (int16_t)(radius_x - 8), (int16_t)(radius_y - 2), RGB565(12, 18, 28));
    }
}

static void draw_spine_spikes(int16_t start_x,
                              int16_t start_y,
                              uint8_t count,
                              int16_t step_x,
                              int16_t rise,
                              color_t fill,
                              color_t outline)
{
    uint8_t index;

    for (index = 0U; index < count; index++) {
        int16_t spike_x = (int16_t)(start_x + ((int16_t)index * step_x));
        int16_t spike_y = (int16_t)(start_y - ((index % 2U == 0U) ? rise : (rise / 2)));
        int16_t radius_x = (int16_t)(4 - ((index > 2U) ? 1 : 0));
        int16_t radius_y = (int16_t)(7 - ((index > 1U) ? 1 : 0));

        if (radius_x < 2) {
            radius_x = 2;
        }
        if (radius_y < 4) {
            radius_y = 4;
        }

        draw_outlined_ellipse(spike_x, spike_y, radius_x, radius_y, 1, fill, outline);
    }
}

static void draw_leg_pose(int16_t hip_x,
                          int16_t hip_y,
                          int16_t knee_x,
                          int16_t knee_y,
                          int16_t foot_x,
                          int16_t foot_y,
                          color_t body,
                          color_t claw,
                          color_t outline)
{
    draw_outlined_ellipse((int16_t)((hip_x + knee_x) / 2),
                          (int16_t)((hip_y + knee_y) / 2),
                          5,
                          11,
                          2,
                          body,
                          outline);
    draw_outlined_ellipse((int16_t)((knee_x + foot_x) / 2),
                          (int16_t)((knee_y + foot_y) / 2),
                          4,
                          10,
                          1,
                          body,
                          outline);
    draw_outlined_ellipse(foot_x, foot_y, 8, 3, 1, claw, outline);
}

static void draw_dragon_egg(color_t accent, uint32_t tick)
{
    int16_t wobble = triangle_wave(tick + 2U, 30U, 2);
    int16_t lift = triangle_wave(tick + 7U, 34U, 1);
    int16_t shard_shift = triangle_wave(tick + 11U, 40U, 2);
    uint16_t crack_glow = positive_wave(tick + 3U, 26U, 3);
    color_t shell_base = RGB565(196, 204, 214);
    color_t shell_shadow = RGB565(118, 132, 148);
    color_t shell_outline = RGB565(22, 30, 44);
    color_t shell_highlight = RGB565(244, 246, 242);
    color_t hatch_dark = RGB565(60, 112, 170);
    color_t hatch_light = RGB565(122, 194, 244);

    /* Bold cracked egg with a visible hatchling face so stage 0 is still clearly dragon-themed. */
    draw_dragon_shadow((int16_t)(238 + wobble), 214, 70, 10);
    draw_outlined_ellipse((int16_t)(238 + wobble), (int16_t)(148 - lift), 62, 86, 2, shell_base, shell_outline);
    draw_outlined_ellipse((int16_t)(230 + wobble), (int16_t)(146 - lift), 50, 74, 2, shell_shadow, shell_outline);
    draw_soft_ellipse((int16_t)(222 + wobble), (int16_t)(132 - lift), 15, 50, shell_highlight);

    draw_horizontal_stack((int16_t)(212 + wobble), (int16_t)(124 - lift), 18, 3U, shell_outline);
    draw_horizontal_stack((int16_t)(228 + wobble), (int16_t)(116 - lift), 22, 3U, shell_outline);
    draw_horizontal_stack((int16_t)(248 + wobble), (int16_t)(110 - lift), 18, 3U, shell_outline);
    draw_horizontal_stack((int16_t)(264 + wobble), (int16_t)(120 - lift), 14, 3U, shell_outline);
    draw_diagonal_stroke((int16_t)(236 + wobble), (int16_t)(106 - lift), 42, -10, 2U, shell_outline);
    draw_diagonal_stroke((int16_t)(252 + wobble), (int16_t)(110 - lift), 34, 9, 2U, shell_outline);

    draw_outlined_ellipse((int16_t)(252 + wobble), (int16_t)(132 - lift), 16, 12, 2, hatch_light, shell_outline);
    draw_outlined_ellipse((int16_t)(262 + wobble), (int16_t)(135 - lift), 8, 6, 1, hatch_dark, shell_outline);
    draw_cartoon_eye((int16_t)(248 + wobble), (int16_t)(131 - lift), false, shell_outline, RGB565(255, 250, 236), RGB565(30, 56, 84));
    draw_outlined_ellipse((int16_t)(270 + wobble), (int16_t)(132 - lift), 6, 5, 1, hatch_light, shell_outline);
    draw_outlined_ellipse((int16_t)(244 + wobble), (int16_t)(166 - lift), 15, 10, 1, hatch_dark, shell_outline);
    draw_outlined_ellipse((int16_t)(228 + wobble), (int16_t)(174 - lift), 10, 8, 1, hatch_dark, shell_outline);
    draw_horizontal_stack((int16_t)(254 + wobble), (int16_t)(143 - lift), (int16_t)(8 + crack_glow), 2U, accent);

    draw_outlined_ellipse((int16_t)(188 + shard_shift), 216, 11, 6, 1, shell_shadow, shell_outline);
    draw_outlined_ellipse((int16_t)(210 - shard_shift), 222, 8, 5, 1, shell_base, shell_outline);
    draw_outlined_ellipse((int16_t)(282 + shard_shift), 218, 13, 6, 1, shell_shadow, shell_outline);
    draw_outlined_ellipse((int16_t)(300 - shard_shift), 224, 8, 4, 1, shell_base, shell_outline);
}

static void draw_dragon_hatchling(color_t accent,
                                  uint32_t tick,
                                  dragon_action_t action,
                                  bool blink)
{
    int16_t bob = triangle_wave(tick, 28U, 1);
    int16_t wing_shift = triangle_wave(tick + 3U, 24U, (action == DRAGON_ACTION_PLAY) ? 6 : 4);
    int16_t tail_shift = triangle_wave(tick + 5U, 28U, 6);
    int16_t head_lift = triangle_wave(tick + 9U, 24U, 2);
    int16_t neck_sway = triangle_wave(tick + 7U, 24U, 1);
    uint16_t breath = positive_wave(tick + 1U, 22U, 2);
    uint16_t paw_lift = positive_wave(tick + 6U, 20U, 1);
    int16_t gaze_shift = triangle_wave(tick + 11U, 26U, 1);
    uint16_t tail_flick = positive_wave(tick + 13U, 18U, (action == DRAGON_ACTION_EXPLORE) ? 3 : 1);
    bool jaw_open = (action == DRAGON_ACTION_PLAY || action == DRAGON_ACTION_EXPLORE);
    color_t outline = RGB565(12, 20, 30);
    color_t wing_membrane = RGB565(54, 124, 110);
    color_t body_mid = RGB565(72, 186, 162);
    color_t body_light = RGB565(146, 236, 212);
    color_t belly = RGB565(248, 236, 196);
    color_t snout = RGB565(236, 218, 178);
    color_t claw = RGB565(246, 238, 214);
    color_t wing_bone = RGB565(216, 248, 234);

    draw_dragon_shadow(232, 212, 78, 12);

    draw_outlined_wing((int16_t)(212 - wing_shift), (int16_t)(94 - bob), 52, 58, 10, true, wing_membrane, outline);
    draw_outlined_wing((int16_t)(270 + wing_shift), (int16_t)(92 - bob), 50, 56, 10, false, wing_membrane, outline);
    draw_wing_bones((int16_t)(212 - wing_shift), (int16_t)(94 - bob), 52, 58, true, wing_bone);
    draw_wing_bones((int16_t)(270 + wing_shift), (int16_t)(92 - bob), 50, 56, false, wing_bone);

    draw_outlined_ellipse((int16_t)(182 - tail_shift), (int16_t)(164 - bob), 22, 9, 2, body_mid, outline);
    draw_outlined_ellipse((int16_t)(160 - tail_shift), (int16_t)(172 - bob), 15, 6, 2, body_mid, outline);
    draw_outlined_ellipse((int16_t)(142 - tail_shift), (int16_t)(178 - bob - tail_flick), 10, 5, 1, accent, outline);
    draw_outlined_ellipse((int16_t)(130 - tail_shift), (int16_t)(181 - bob - tail_flick), 5, 3, 1, claw, outline);

    draw_outlined_ellipse(230, (int16_t)(156 - bob), 34, (int16_t)(21 + (breath / 2U)), 2, body_mid, outline);
    draw_outlined_ellipse(222, (int16_t)(144 - bob), 20, 12, 2, body_light, outline);
    draw_outlined_ellipse(236, (int16_t)(166 - bob), 16, 10, 2, belly, outline);
    draw_spine_spikes(206, (int16_t)(132 - bob), 4U, 13, 12, accent, outline);
    draw_horizontal_stack(220, (int16_t)(162 - bob), 18, 1U, RGB565(230, 202, 146));
    draw_horizontal_stack(224, (int16_t)(170 - bob), 12, 1U, RGB565(230, 202, 146));

    draw_outlined_ellipse((int16_t)(266 + neck_sway), (int16_t)(140 - bob - head_lift), 10, 14, 2, body_mid, outline);
    draw_outlined_ellipse((int16_t)(294 + neck_sway), (int16_t)(121 - bob - head_lift), 22, 16, 2, body_light, outline);
    draw_outlined_ellipse((int16_t)(312 + neck_sway), (int16_t)(124 - bob - head_lift), 10, 8, 2, snout, outline);
    draw_outlined_ellipse((int16_t)(298 + neck_sway), (int16_t)(138 - bob - head_lift), 11, 5, 2, snout, outline);
    draw_spine_spikes((int16_t)(274 + neck_sway), (int16_t)(108 - bob - head_lift), 3U, 10, 10, accent, outline);

    if (jaw_open) {
        draw_horizontal_span((int16_t)(311 + neck_sway), (int16_t)(136 - bob - neck_sway - head_lift), 12, RGB565(72, 24, 20));
        draw_horizontal_span((int16_t)(313 + neck_sway), (int16_t)(137 - bob - neck_sway - head_lift), 7, COLOR_WHITE);
        draw_outlined_ellipse((int16_t)(329 + neck_sway), (int16_t)(136 - bob - neck_sway - head_lift), 6, 3, 1, RGB565(255, 168, 112), outline);
    }

    draw_outlined_ellipse(212, (int16_t)(132 - bob), 4, 6, 1, accent, outline);
    draw_outlined_ellipse(224, (int16_t)(126 - bob), 4, 6, 1, accent, outline);
    draw_outlined_ellipse((int16_t)(272 + neck_sway), (int16_t)(112 - bob - head_lift), 4, 5, 1, accent, outline);
    draw_outlined_ellipse((int16_t)(284 + neck_sway), (int16_t)(108 - bob - head_lift), 4, 5, 1, accent, outline);
    draw_cartoon_eye((int16_t)(291 + neck_sway + gaze_shift),
                     (int16_t)(126 - bob - head_lift),
                     blink,
                     outline,
                     RGB565(220, 220, 240),
                     RGB565(120, 120, 140));
    if (jaw_open) {
        draw_outlined_ellipse((int16_t)(318 + neck_sway),
                              (int16_t)(141 - bob - head_lift),
                              5,
                              3,
                              1,
                              RGB565(220, 220, 240),
                              outline);
    }
    draw_leg_pose(214,
                  (int16_t)(171 - bob),
                  214,
                  (int16_t)(188 - bob + paw_lift),
                  218,
                  (int16_t)(201 - bob),
                  body_mid,
                  claw,
                  outline);
    draw_leg_pose(246,
                  (int16_t)(172 - bob),
                  248,
                  (int16_t)(192 - bob),
                  252,
                  (int16_t)(202 - bob),
                  body_mid,
                  claw,
                  outline);
    draw_leg_pose(198,
                  (int16_t)(168 - bob),
                  192,
                  (int16_t)(188 - bob),
                  194,
                  (int16_t)(206 - bob),
                  body_mid,
                  claw,
                  outline);

    if (action == DRAGON_ACTION_EXPLORE) {
        draw_outlined_ellipse((int16_t)(332 + neck_sway), (int16_t)(120 - bob - head_lift), 4, 4, 1, accent, outline);
        draw_outlined_ellipse((int16_t)(340 + neck_sway), (int16_t)(116 - bob - head_lift), 2, 2, 1, RGB565(255, 238, 214), outline);
    } else if (action == DRAGON_ACTION_PLAY) {
        draw_outlined_ellipse((int16_t)(262 - tail_shift), (int16_t)(198 - bob), 6, 4, 1, accent, outline);
        draw_outlined_ellipse((int16_t)(272 - tail_shift), (int16_t)(192 - bob), 3, 3, 1, RGB565(255, 234, 196), outline);
    }
}

static void draw_dragon_adult(color_t accent,
                              uint32_t tick,
                              dragon_action_t action,
                              bool blink)
{
    int16_t bob = triangle_wave(tick, 28U, 2); // Smoother bob
    int16_t wing_shift = triangle_wave(tick + 5U, 22U, (action == DRAGON_ACTION_EXPLORE) ? 14 : 8); // Larger wing sweep
    int16_t tail_shift = triangle_wave(tick + 9U, 30U, 12); // More tail motion
    int16_t neck_shift = triangle_wave(tick + 3U, 26U, 4); // More neck sway
    int16_t head_tilt = triangle_wave(tick + 15U, 30U, 4); // More head tilt
    uint16_t breath = positive_wave(tick + 2U, 22U, 4); // Deeper breathing
    uint16_t claw_step = positive_wave(tick + 7U, 20U, 2); // More claw movement
    uint16_t wing_crest = positive_wave(tick + 8U, 18U, (action == DRAGON_ACTION_EXPLORE) ? 8 : 4); // More wing crest
    int16_t gaze_shift = triangle_wave(tick + 17U, 24U, 2); // More eye movement
    bool jaw_open = (action == DRAGON_ACTION_EXPLORE || action == DRAGON_ACTION_PLAY);
    color_t outline = RGB565(18, 24, 32); // black chrome outline
    color_t wing_membrane = RGB565(60, 60, 80); // dark chrome
    color_t body_mid = RGB565(32, 32, 48); // black scales
    color_t body_light = RGB565(120, 120, 140); // chrome highlights
    color_t belly = RGB565(80, 80, 100); // dark chrome belly
    color_t snout = RGB565(140, 140, 160); // chrome snout
    color_t claw = RGB565(220, 220, 240); // chrome claws
    color_t wing_bone = RGB565(180, 180, 200); // chrome wing bones

    // Center dragon, avoid info overlap
    draw_dragon_shadow(238, 170, 92, 12);
    // Animated wing flapping
    draw_outlined_wing((int16_t)(170 - wing_shift), (int16_t)(70 - bob - (int16_t)(wing_crest / 2U)), 82, 92, 14, true, wing_membrane, outline);
    draw_outlined_wing((int16_t)(306 + wing_shift), (int16_t)(70 - bob - (int16_t)(wing_crest / 2U)), 82, 92, 14, false, wing_membrane, outline);
    draw_wing_bones((int16_t)(170 - wing_shift), (int16_t)(70 - bob - (int16_t)(wing_crest / 2U)), 82, 92, true, wing_bone);
    draw_wing_bones((int16_t)(306 + wing_shift), (int16_t)(70 - bob - (int16_t)(wing_crest / 2U)), 82, 92, false, wing_bone);
    // Animated tail
    draw_outlined_ellipse((int16_t)(170 - tail_shift), (int16_t)(140 - bob), 28, 10, 2, body_mid, outline);
    draw_outlined_ellipse((int16_t)(146 - tail_shift), (int16_t)(148 - bob), 18, 7, 2, body_mid, outline);
    draw_outlined_ellipse((int16_t)(118 - tail_shift), (int16_t)(156 - bob), 11, 5, 2, accent, outline);
    draw_outlined_ellipse((int16_t)(106 - tail_shift), (int16_t)(160 - bob), 5, 3, 1, claw, outline);
    // Breathing effect
    draw_outlined_ellipse(238, (int16_t)(120 - bob), 52, (int16_t)(28 + (breath / 2U)), 2, body_mid, outline);
    draw_outlined_ellipse(228, (int16_t)(106 - bob), 30, 17, 2, body_light, outline);
    draw_outlined_ellipse(248, (int16_t)(132 - bob), 20, 11, 2, belly, outline);
    // Chrome scale shimmer
    if ((tick % 16U) < 8U) {
        draw_horizontal_stack(224, (int16_t)(126 - bob), 22, 1U, RGB565(180, 180, 200));
        draw_horizontal_stack(228, (int16_t)(136 - bob), 18, 1U, RGB565(220, 220, 240));
        draw_horizontal_stack(232, (int16_t)(146 - bob), 12, 1U, RGB565(120, 120, 140));
    } else {
        draw_horizontal_stack(224, (int16_t)(126 - bob), 22, 1U, RGB565(120, 120, 140));
        draw_horizontal_stack(228, (int16_t)(136 - bob), 18, 1U, RGB565(180, 180, 200));
        draw_horizontal_stack(232, (int16_t)(146 - bob), 12, 1U, RGB565(220, 220, 240));
    }
    // Animated neck and head
    draw_outlined_ellipse((int16_t)(278 + neck_shift), (int16_t)(96 - bob), 13, 18, 2, body_mid, outline);
    draw_outlined_ellipse((int16_t)(312 + neck_shift), (int16_t)(76 - bob - neck_shift - head_tilt), 24, 18, 2, body_light, outline);
    draw_outlined_ellipse((int16_t)(330 + neck_shift), (int16_t)(80 - bob - neck_shift - head_tilt), 11, 8, 2, snout, outline);
    draw_outlined_ellipse((int16_t)(316 + neck_shift), (int16_t)(94 - bob - neck_shift - head_tilt), 12, 6, 2, snout, outline);
    draw_spine_spikes((int16_t)(298 + neck_shift), (int16_t)(52 - bob - neck_shift), 3U, 11, 12, accent, outline);
    // Expressive face
    if (jaw_open) {
        draw_horizontal_span((int16_t)(315 + neck_shift), (int16_t)(102 - bob - neck_shift - head_tilt), 12, RGB565(120, 120, 140));
        draw_horizontal_span((int16_t)(317 + neck_shift), (int16_t)(103 - bob - neck_shift - head_tilt), 7, COLOR_WHITE);
        draw_outlined_ellipse((int16_t)(333 + neck_shift), (int16_t)(102 - bob - neck_shift - head_tilt), 6, 3, 1, RGB565(220, 220, 240), outline);
    }
    draw_outlined_ellipse(222, (int16_t)(84 - bob), 6, 10, 1, accent, outline);
    draw_outlined_ellipse(238, (int16_t)(70 - bob), 6, 11, 1, accent, outline);
    draw_outlined_ellipse(256, (int16_t)(64 - bob), 6, 10, 1, accent, outline);
    draw_outlined_ellipse((int16_t)(300 + neck_shift), (int16_t)(56 - bob - neck_shift), 5, 9, 1, accent, outline);
    draw_outlined_ellipse((int16_t)(311 + neck_shift), (int16_t)(54 - bob - neck_shift), 5, 9, 1, accent, outline);
    draw_cartoon_eye((int16_t)(306 + neck_shift + gaze_shift),
                     (int16_t)(78 - bob - neck_shift - head_tilt),
                     blink,
                     outline,
                     RGB565(220, 220, 240),
                     RGB565(120, 120, 140));
    // Animated legs
    draw_leg_pose(212,
                  (int16_t)(136 - bob),
                  210,
                  (int16_t)(156 - bob + claw_step),
                  214,
                  (int16_t)(174 - bob),
                  body_mid,
                  claw,
                  outline);
    draw_leg_pose(246,
                  (int16_t)(138 - bob),
                  248,
                  (int16_t)(158 - bob),
                  252,
                  (int16_t)(176 - bob),
                  body_mid,
                  claw,
                  outline);
    draw_leg_pose(198,
                  (int16_t)(134 - bob),
                  192,
                  (int16_t)(154 - bob),
                  194,
                  (int16_t)(172 - bob),
                  body_mid,
                  claw,
                  outline);
    // Fire breathing effect (optional, can be toggled)
    if (action == DRAGON_ACTION_EXPLORE && (tick % 32U) < 16U) {
        draw_outlined_ellipse((int16_t)(346 + neck_shift), (int16_t)(98 - bob - neck_shift - head_tilt), 14, 6, 1, RGB565(255, 120, 40), outline);
    }
    if (action == DRAGON_ACTION_EXPLORE) {
        draw_outlined_ellipse((int16_t)(346 + neck_shift), (int16_t)(98 - bob - neck_shift - head_tilt), 7, 5, 1, RGB565(120, 120, 140), outline);
        draw_outlined_ellipse((int16_t)(356 + neck_shift), (int16_t)(94 - bob - neck_shift - head_tilt), 4, 3, 1, accent, outline);
        draw_outlined_ellipse((int16_t)(364 + neck_shift), (int16_t)(90 - bob - neck_shift - head_tilt), 2, 2, 1, RGB565(220, 220, 240), outline);
    } else if (action == DRAGON_ACTION_PLAY) {
        draw_outlined_ellipse((int16_t)(266 - tail_shift), (int16_t)(164 - bob), 6, 4, 1, accent, outline);
        draw_outlined_ellipse((int16_t)(276 - tail_shift), (int16_t)(158 - bob), 3, 3, 1, RGB565(220, 220, 240), outline);
    }
}

static void draw_boot_emblem(uint16_t pulse, color_t outline, color_t glow)
{
    uint16_t left_x = (uint16_t)(168U - (pulse / 4U));
    uint16_t right_x = (uint16_t)(240U + (pulse / 4U));

    tft_fill_rect(118, 44, 244, 190, RGB565(10, 14, 20));
    tft_fill_rect(132, 58, 216, 162, RGB565(14, 20, 28));

    tft_fill_rect(left_x, 78, 56, 16, glow);
    tft_fill_rect(left_x - 12, 94, 24, 18, glow);
    tft_fill_rect(left_x + 44, 94, 24, 18, glow);
    tft_fill_rect(left_x - 4, 112, 14, 18, outline);
    tft_fill_rect(left_x + 50, 112, 14, 18, outline);
    tft_fill_rect(left_x + 8, 126, 12, 18, outline);
    tft_fill_rect(left_x + 38, 126, 12, 18, outline);
    tft_fill_rect(190, 136, 16, 18, outline);
    tft_fill_rect(204, 152, 14, 18, outline);
    tft_fill_rect(216, 168, 12, 18, outline);

    tft_fill_rect(right_x, 78, 56, 16, outline);
    tft_fill_rect(right_x - 12, 94, 24, 18, outline);
    tft_fill_rect(right_x + 44, 94, 24, 18, outline);
    tft_fill_rect(right_x - 2, 112, 14, 18, outline);
    tft_fill_rect(right_x + 48, 112, 14, 18, outline);
    tft_fill_rect(264, 126, 12, 18, outline);
    tft_fill_rect(288, 126, 12, 18, outline);
    tft_fill_rect(252, 144, 14, 16, outline);
    tft_fill_rect(300, 144, 14, 16, outline);

    tft_fill_rect(156, 168, 164, 12, outline);
    tft_fill_rect(144, 180, 24, 10, outline);
    tft_fill_rect(308, 180, 24, 10, outline);
    tft_fill_rect(176, 146, 12, 16, outline);
    tft_fill_rect(188, 132, 12, 16, outline);
    tft_fill_rect(200, 118, 12, 16, outline);
    tft_fill_rect(212, 104, 12, 16, outline);
    tft_fill_rect(224, 92, 12, 16, outline);
    tft_fill_rect(236, 104, 12, 16, outline);
    tft_fill_rect(248, 118, 12, 16, outline);
    tft_fill_rect(260, 132, 12, 16, outline);
    tft_fill_rect(272, 146, 12, 16, outline);

    tft_fill_rect(224, 178, 12, 18, outline);
    tft_fill_rect(216, 194, 14, 18, outline);
    tft_fill_rect(208, 210, 14, 18, outline);
    tft_fill_rect(238, 194, 14, 18, outline);
    tft_fill_rect(246, 210, 14, 18, outline);
    tft_fill_rect(228, 226, 12, 20, outline);

    tft_fill_rect(220, 116, 20, 6, COLOR_BLACK);
    tft_fill_rect(208, 132, 44, 6, COLOR_BLACK);
    tft_fill_rect(198, 148, 64, 6, COLOR_BLACK);
    tft_fill_rect(190, 164, 80, 6, COLOR_BLACK);

    tft_fill_rect(170, 74, 52, 6, COLOR_BLACK);
    tft_fill_rect(254, 74, 52, 6, COLOR_BLACK);
}

static void play_boot_animation(void)
{
    static const color_t accents[] = {
        RGB565(222, 62, 72),
        RGB565(188, 42, 54),
        RGB565(235, 230, 218),
        RGB565(148, 24, 32)
    };
    uint16_t step;

    if (!app_state.display_ready) {
        if (tft_clear() != HAL_OK) {
            return;
        }
        app_state.display_ready = 1;
    }

    for (step = 0U; step < 14U && !should_exit; step++) {
        color_t accent = accents[step % (sizeof(accents) / sizeof(accents[0]))];
        uint16_t progress = (uint16_t)(((step + 1U) * 296U) / 14U);
        uint16_t sweep = (uint16_t)(54U + (step * 24U));
        uint16_t pulse = (uint16_t)((step % 4U) * 4U);
        bool show_emblem = (step >= 2U);
        bool show_scan = (step >= 4U);
        bool show_eyes = (step >= 8U);
        bool show_title = (step >= 10U);

        tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, RGB565(6, 8, 12));
        tft_fill_rect(0, 0, TFT_WIDTH, 8, accent);
        tft_fill_rect(0, 210, TFT_WIDTH, 110, RGB565(12, 8, 10));
        tft_fill_rect(60, 34, 360, 252, RGB565(10, 12, 18));
        if (show_scan) {
            tft_fill_rect(sweep, 58, 10, 140, RGB565(42, 12, 16));
            tft_fill_rect((uint16_t)(sweep + 18U), 74, 6, 112, RGB565(84, 22, 26));
        }
        if (show_emblem) {
            draw_boot_emblem(pulse, COLOR_WHITE, accent);
        }
        if (show_eyes) {
            tft_fill_rect(188, 132, 18, 6, accent);
            tft_fill_rect(274, 132, 18, 6, accent);
            tft_fill_rect(192, 134, 10, 2, COLOR_WHITE);
            tft_fill_rect(278, 134, 10, 2, COLOR_WHITE);
        }
        if (show_title) {
            draw_text_line(170, 52, "LOKI", COLOR_WHITE, 3);
            draw_text_line(142, 248, "AWAKENING THE NEST", accent, 1);
            draw_text_line(128, 264, "DISPLAY AI WEB SYSTEMS", COLOR_WHITE, 1);
        }
        tft_fill_rect(92, 286, 296, 12, RGB565(36, 44, 58));
        tft_fill_rect(92, 286, progress, 12, accent);
        DELAY_MS((step < 4U) ? 70 : 95);
    }
}

static hal_status_t render_dragon_dashboard(const dragon_ai_state_t *dragon)
{
    color_t accent;
    char ip_address[32];
    char gps_lat_line[48];
    char gps_long_line[48];
    char gps_alt_line[32];
    char stage_line[48];
    char action_line[64];
    char ai_line[64];
    char scan_line[64];
    char reward_line[32];
    uint16_t shell_pulse;
    uint16_t sweep_left;
    uint16_t sweep_top;
    uint16_t scan_progress;
    uint16_t motion_glow;
    bool cursor_visible;
    bool blink;

    if (dragon == NULL) {
        return HAL_INVALID_PARAM;
    }
    if (!app_state.display_ready) {
        return HAL_OK;
    }

    accent = mood_accent_color(dragon->mood);
    shell_pulse = (uint16_t)(6U + ((dragon->age_ticks % 8U) * 2U));
    sweep_left = (uint16_t)(30U + ((dragon->age_ticks * 11U) % 220U));
    sweep_top = (uint16_t)(56U + ((dragon->age_ticks * 7U) % 96U));
    motion_glow = positive_wave(dragon->age_ticks + (uint32_t)dragon->action, 18U, 16U);
    scan_progress = 0U;
    if (app_state.config.scan_interval_ms > 0U) {
        scan_progress = (uint16_t)((app_state.network_phase_ms * 144U) / app_state.config.scan_interval_ms);
        if (scan_progress > 144U) {
            scan_progress = 144U;
        }
    }
    cursor_visible = ((dragon->age_ticks % 4U) < 2U);
    blink = (((dragon->age_ticks + (uint32_t)dragon->action) % 17U) == 0U) ||
            (((dragon->age_ticks + (uint32_t)dragon->mood) % 19U) == 1U);
    refresh_gps_state();
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
    if (app_state.gps.has_fix) {
        snprintf(gps_lat_line, sizeof(gps_lat_line), "LAT %.5f", app_state.gps.latitude);
        snprintf(gps_long_line, sizeof(gps_long_line), "LONG %.5f", app_state.gps.longitude);
        snprintf(gps_alt_line, sizeof(gps_alt_line), "ALT %.1f M", app_state.gps.altitude_m);
    } else {
        copy_text(gps_lat_line, sizeof(gps_lat_line), "LAT --");
        copy_text(gps_long_line, sizeof(gps_long_line), "LONG --");
        copy_text(gps_alt_line, sizeof(gps_alt_line), "ALT --");
    }

    // Modern dark background with subtle gradient
    tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, RGB565(10, 14, 22));
    tft_fill_rect(0, 0, TFT_WIDTH, 8, RGB565(34, 42, 54));
    tft_fill_rect(0, TFT_HEIGHT - 16, TFT_WIDTH, 16, RGB565(18, 24, 32));

    // Sleek info panels with rounded corners

    // Clear display and set premium background
    tft_clear();
    tft_fill_rect(0, 0, 480, 320, RGB565(18, 24, 32)); // Full dark background
    tft_fill_rect(16, 16, 448, 288, RGB565(24, 32, 48)); // Main panel
    tft_fill_rect(32, 32, 200, 120, RGB565(28, 36, 54)); // Info panel left
    tft_fill_rect(248, 32, 200, 120, RGB565(28, 36, 54)); // Info panel right
    tft_fill_rect(32, 168, 416, 120, RGB565(22, 28, 40)); // Stats panel

    // Info overlays: IP, GPS, unobstructed
    draw_text_line(40, 44, "IP", accent, 2);
    draw_text_line(80, 44, ip_address, COLOR_WHITE, 2);
    draw_text_line(40, 72, "GPS", accent, 2);
    draw_text_line(80, 72, gps_lat_line, COLOR_WHITE, 2);
    draw_text_line(80, 96, gps_long_line, COLOR_WHITE, 2);
    draw_text_line(80, 120, gps_alt_line, COLOR_WHITE, 2);

    // Dragon info, stage, speech
    draw_text_line(256, 44, "LOKI", accent, 3);
    draw_text_line(256, 76, stage_line, RGB565(148, 214, 255), 2);
    draw_text_block(256, 102, 180, dragon->speech, COLOR_WHITE, 1, 4);

    // System info
    draw_text_line(256, 136, "SYSTEM", COLOR_WHITE, 2);
    draw_text_line(256, 164, action_line, COLOR_WHITE, 2);
    draw_text_line(256, 188, ai_line, RGB565(148, 214, 255), 2);
    draw_text_line(256, 212, scan_line, RGB565(124, 255, 194), 2);
    draw_text_line(256, 236, reward_line, RGB565(255, 202, 112), 2);
    char tool_count_line[48];
    snprintf(tool_count_line, sizeof(tool_count_line), "TOOLS RUNNING: %u", (unsigned int)app_state.running_tool_count);
    draw_text_line(256, 260, tool_count_line, RGB565(255, 120, 110), 2);
    char ai_mode_line[48];
    snprintf(ai_mode_line, sizeof(ai_mode_line), "AI MODE: %s", app_state.config.dragon_ai ? "ON" : "OFF");
    draw_text_line(256, 284, ai_mode_line, RGB565(120, 215, 255), 2);

    // Animated progress bar
    tft_fill_rect(256, 310, 180, 8, RGB565(34, 42, 54));
    tft_fill_rect(256, 310, scan_progress, 8, accent);
    if (cursor_visible) {
        tft_fill_rect(436, 318, 8, 4, accent);
    }

    // Stats bars with gradients, unobstructed
    draw_text_line(48, 180, "HUNGER", COLOR_WHITE, 2);
    tft_fill_rect(48, 196, 116, 12, RGB565(40, 50, 66));
    tft_fill_rect(48, 196, bar_width_from_ratio(1.0f - dragon->hunger), 12, RGB565(255, 120, 110));
    draw_text_line(48, 212, "ENERGY", COLOR_WHITE, 2);
    tft_fill_rect(48, 228, 116, 12, RGB565(40, 50, 66));
    tft_fill_rect(48, 228, bar_width_from_ratio(dragon->energy), 12, RGB565(120, 215, 255));
    draw_text_line(176, 180, "CURIOUS", COLOR_WHITE, 2);
    tft_fill_rect(176, 196, 116, 12, RGB565(40, 50, 66));
    tft_fill_rect(176, 196, bar_width_from_ratio(dragon->curiosity), 12, RGB565(125, 255, 184));
    draw_text_line(176, 212, "BOND", COLOR_WHITE, 2);
    tft_fill_rect(176, 228, 116, 12, RGB565(40, 50, 66));
    tft_fill_rect(176, 228, bar_width_from_ratio(dragon->bond), 12, RGB565(255, 221, 87));
    draw_discovery_ticker(dragon->age_ticks, accent);

    // Centered, visually distinct dragon animation with chrome highlights and expressive face
    // Draw a shadow under the dragon for separation
    tft_fill_rect(220, 260, 40, 12, RGB565(32, 32, 48));

    switch (dragon_ai_life_stage(dragon->growth_stage)) {
        case DRAGON_LIFE_STAGE_EGG:
            draw_dragon_egg(RGB565(196, 204, 214), dragon->age_ticks); // Brighter shell
            break;
        case DRAGON_LIFE_STAGE_HATCHLING:
            draw_dragon_hatchling(RGB565(122, 194, 244), dragon->age_ticks, dragon->action, blink); // Blue chrome
            break;
        case DRAGON_LIFE_STAGE_ADULT:
        default:
            // Use black scales, chrome highlights, sharp outline, expressive face
            draw_dragon_adult(RGB565(120, 120, 140), dragon->age_ticks, dragon->action, blink);
            // Add chrome scale highlights
            tft_fill_rect(240, 120, 12, 8, RGB565(220, 220, 240));
            tft_fill_rect(260, 140, 8, 6, RGB565(180, 180, 200));
            tft_fill_rect(280, 160, 10, 7, RGB565(255, 255, 255));
            // Draw expressive eyes
            tft_fill_rect(270, 110, 6, 6, RGB565(255, 255, 255));
            tft_fill_rect(272, 112, 2, 2, RGB565(0, 0, 0));
            // Draw a smile
            tft_fill_rect(275, 118, 8, 2, RGB565(255, 120, 110));
            break;
    }
    // Ensure info overlays are always readable
    tft_fill_rect(32, 32, 200, 120, RGB565(28, 36, 54)); // Redraw info panel left
    tft_fill_rect(248, 32, 200, 120, RGB565(28, 36, 54)); // Redraw info panel right
    draw_text_line(40, 44, "IP", accent, 2);
    draw_text_line(80, 44, ip_address, COLOR_WHITE, 2);
    draw_text_line(40, 72, "GPS", accent, 2);
    draw_text_line(80, 72, gps_lat_line, COLOR_WHITE, 2);
    draw_text_line(80, 96, gps_long_line, COLOR_WHITE, 2);
    draw_text_line(80, 120, gps_alt_line, COLOR_WHITE, 2);
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
    if (app_state.config.standalone_mode) {
        set_discovery_message("OFFLINE SCOUT MODE - WAITING FOR NEW NETWORKS", 0U);
    }
    refresh_nearby_networks();
    LOG_INFO("Network state plugin active (dhcp=%s, standalone=%s, scan=%u ms, hop=%u ms)",
             app_state.config.dhcp_enabled ? "on" : "off",
             app_state.config.standalone_mode ? "on" : "off",
             app_state.config.scan_interval_ms,
             app_state.config.hop_interval_ms);
    return HAL_OK;
}

static hal_status_t network_state_plugin_update(loki_plugin_t *plugin, uint32_t tick_ms)
{
    (void)plugin;

    if (app_state.discovery_hold_remaining_ms > 0U) {
        if (tick_ms >= app_state.discovery_hold_remaining_ms) {
            app_state.discovery_hold_remaining_ms = 0U;
            if (app_state.config.standalone_mode) {
                set_discovery_message("OFFLINE SCOUT MODE - WAITING FOR NEW NETWORKS", 0U);
            } else {
                app_state.discovery_message[0] = '\0';
            }
        } else {
            app_state.discovery_hold_remaining_ms -= tick_ms;
        }
    }

    app_state.network_phase_ms += tick_ms;
    if (app_state.config.scan_interval_ms > 0 &&
        app_state.network_phase_ms >= app_state.config.scan_interval_ms) {
        app_state.network_phase_ms = 0;
        refresh_nearby_networks();
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
                        handle_api_request,
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

    refresh_gps_state();
    get_primary_ip_address(ip_address, sizeof(ip_address));

    copy_text(snapshot.device_name, sizeof(snapshot.device_name), app_state.config.device_name);
    copy_text(snapshot.dragon_name, sizeof(snapshot.dragon_name), app_state.config.dragon_name);
    copy_text(snapshot.hostname, sizeof(snapshot.hostname), app_state.config.hostname);
    copy_text(snapshot.primary_ip, sizeof(snapshot.primary_ip), ip_address);
    copy_text(snapshot.dragon_profile, sizeof(snapshot.dragon_profile), app_state.config.dragon.profile);
    copy_text(snapshot.dragon_temperament, sizeof(snapshot.dragon_temperament), app_state.config.dragon.temperament);
    copy_text(snapshot.gps_source, sizeof(snapshot.gps_source), app_state.gps.source);
    copy_text(snapshot.nearby_networks, sizeof(snapshot.nearby_networks), app_state.nearby_networks);
    copy_text(snapshot.discovery_message, sizeof(snapshot.discovery_message), app_state.discovery_message);
    copy_text(snapshot.preferred_ssid, sizeof(snapshot.preferred_ssid), app_state.config.preferred_ssid);
    copy_text(snapshot.bind_address, sizeof(snapshot.bind_address), app_state.config.web.bind_address);
    copy_text(snapshot.display_backend, sizeof(snapshot.display_backend), app_state.config.display.backend);
    copy_text(snapshot.framebuffer_device, sizeof(snapshot.framebuffer_device), app_state.config.display.framebuffer_device);
    snapshot.port = app_state.config.web.port;
    snapshot.heartbeat_ticks = app_state.heartbeat_ticks;
    snapshot.network_phase_ms = app_state.network_phase_ms;
    snapshot.nearby_network_count = app_state.nearby_network_count;
    snapshot.known_network_total = count_network_list_entries(app_state.known_networks);
    snapshot.scan_interval_ms = app_state.config.scan_interval_ms;
    snapshot.hop_interval_ms = app_state.config.hop_interval_ms;
    snapshot.ai_tick_interval_ms = app_state.config.ai.tick_interval_ms;
    snapshot.dhcp_enabled = app_state.config.dhcp_enabled;
    snapshot.standalone_mode = app_state.config.standalone_mode;
    snapshot.announce_new_networks = app_state.config.announce_new_networks;
    snapshot.web_ui_enabled = app_state.config.web_ui_enabled;
    snapshot.display_ready = (app_state.display_ready != 0U);
    snapshot.gps_has_fix = app_state.gps.has_fix;
    snapshot.plugin_display = app_state.config.plugins.display;
    snapshot.plugin_dragon_ai = app_state.config.plugins.dragon_ai;
    snapshot.plugin_network_state = app_state.config.plugins.network_state;
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
    snapshot.dragon_base_energy = app_state.config.dragon.base_energy;
    snapshot.dragon_base_curiosity = app_state.config.dragon.base_curiosity;
    snapshot.dragon_base_bond = app_state.config.dragon.base_bond;
    snapshot.dragon_base_hunger = app_state.config.dragon.base_hunger;
    snapshot.dragon_hunger_rate = app_state.config.dragon.hunger_rate;
    snapshot.dragon_energy_decay = app_state.config.dragon.energy_decay;
    snapshot.dragon_curiosity_rate = app_state.config.dragon.curiosity_rate;
    snapshot.dragon_explore_xp_gain = app_state.config.dragon.explore_xp_gain;
    snapshot.dragon_play_xp_gain = app_state.config.dragon.play_xp_gain;
    snapshot.dragon_growth_xp_step = app_state.config.dragon.growth_xp_step;
    snapshot.gps_latitude = app_state.gps.latitude;
    snapshot.gps_longitude = app_state.gps.longitude;
    snapshot.gps_altitude_m = app_state.gps.altitude_m;
    snapshot.credits_balance = app_state.credits_balance;
    snapshot.running_tool_count = app_state.running_tool_count;
    snapshot.dragon_growth_interval_ms = app_state.config.dragon.growth_interval_ms;
    snapshot.dragon_egg_stage_max = app_state.config.dragon.egg_stage_max;
    snapshot.dragon_hatchling_stage_max = app_state.config.dragon.hatchling_stage_max;
    snapshot.discovery_hold_ms = app_state.config.discovery_hold_ms;
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

#define LOKI_EEPROM_CREDITS_ADDR 0x10U
#define LOKI_EEPROM_CREDITS_MAGIC 0xC3U
#define LOKI_EEPROM_CREDITS_VERSION 1U

typedef struct {
    uint8_t magic;
    uint8_t version;
    uint8_t reserved0;
    uint8_t reserved1;
    uint8_t credits_le[4];
    uint8_t checksum;
} loki_eeprom_credits_record_t;

static uint8_t loki_checksum8(const uint8_t *data, size_t length)
{
    size_t index;
    uint8_t sum = 0U;

    if (data == NULL || length == 0U) {
        return 0U;
    }

    for (index = 0U; index < length; index++) {
        sum = (uint8_t)(sum + data[index]);
    }

    return sum;
}

static hal_status_t loki_eeprom_load_credits(float *credits_out)
{
    loki_eeprom_credits_record_t record;
    uint8_t expected_checksum;
    float credits;

    if (credits_out == NULL) {
        return HAL_INVALID_PARAM;
    }

    *credits_out = 0.0f;
    memset(&record, 0, sizeof(record));

    if (RETRY(eeprom_read(LOKI_EEPROM_CREDITS_ADDR,
                          (uint8_t *)&record,
                          (uint16_t)sizeof(record)),
              RETRY_BALANCED) != HAL_OK) {
        return HAL_ERROR;
    }

    expected_checksum = loki_checksum8((const uint8_t *)&record,
                                       sizeof(record) - 1U);

    if (record.magic != LOKI_EEPROM_CREDITS_MAGIC ||
        record.version != LOKI_EEPROM_CREDITS_VERSION ||
        record.checksum != expected_checksum) {
        return HAL_ERROR;
    }

    memcpy(&credits, record.credits_le, sizeof(credits));
    if (isnan(credits) || isinf(credits)) {
        return HAL_ERROR;
    }

    *credits_out = credits;
    return HAL_OK;
}

static hal_status_t loki_eeprom_save_credits(float credits)
{
    loki_eeprom_credits_record_t record;

    if (isnan(credits) || isinf(credits)) {
        return HAL_INVALID_PARAM;
    }

    memset(&record, 0, sizeof(record));
    record.magic = LOKI_EEPROM_CREDITS_MAGIC;
    record.version = LOKI_EEPROM_CREDITS_VERSION;
    memcpy(record.credits_le, &credits, sizeof(credits));
    record.checksum = loki_checksum8((const uint8_t *)&record,
                                     sizeof(record) - 1U);

    return RETRY(eeprom_write(LOKI_EEPROM_CREDITS_ADDR,
                              (const uint8_t *)&record,
                              (uint16_t)sizeof(record)),
                 RETRY_BALANCED);
}

#define LOKI_EEPROM_FLIPPER_RECORD_ADDR 0xC0U
#define LOKI_EEPROM_FLIPPER_PAYLOAD_MAX 48U
#define LOKI_FLIPPER_TAG_NFC 1U
#define LOKI_FLIPPER_TAG_RFID 2U

static const char *flipper_tag_name(uint8_t tag_type)
{
    if (tag_type == LOKI_FLIPPER_TAG_NFC) {
        return "NFC";
    }
    if (tag_type == LOKI_FLIPPER_TAG_RFID) {
        return "RFID";
    }
    return "UNKNOWN";
}

static hal_status_t persist_flipper_tag_to_eeprom(uint8_t tag_type, const uint8_t *payload, uint16_t payload_length)
{
    uint8_t record[LOKI_EEPROM_FLIPPER_PAYLOAD_MAX + 5U];
    uint16_t stored_length;
    uint8_t checksum;
    uint16_t index;

    if (payload == NULL || payload_length == 0U) {
        return HAL_INVALID_PARAM;
    }

    stored_length = payload_length;
    if (stored_length > LOKI_EEPROM_FLIPPER_PAYLOAD_MAX) {
        stored_length = LOKI_EEPROM_FLIPPER_PAYLOAD_MAX;
    }

    record[0] = 0xA5U;
    record[1] = 0x01U;
    record[2] = tag_type;
    record[3] = (uint8_t)stored_length;
    memcpy(&record[4], payload, stored_length);

    checksum = 0U;
    for (index = 0U; index < (uint16_t)(4U + stored_length); index++) {
        checksum ^= record[index];
    }
    record[4U + stored_length] = checksum;

    return RETRY(eeprom_write((uint8_t)LOKI_EEPROM_FLIPPER_RECORD_ADDR,
                              record,
                              (uint16_t)(5U + stored_length)),
                 RETRY_BALANCED);
}

static void request_flipper_scan_capture(void)
{
    flipper_message_t request = {
        .cmd = FLIPPER_CMD_NFC_SCAN_REQUEST,
        .length = 0,
        .payload = NULL,
    };

    (void)flipper_send_message(&request);
    request.cmd = FLIPPER_CMD_RFID_SCAN_REQUEST;
    (void)flipper_send_message(&request);
}

static void process_flipper_message(const flipper_message_t *msg)
{
    uint8_t tag_type = 0U;
    const uint8_t *tag_payload = NULL;
    uint16_t tag_length = 0U;

    if (msg == NULL) {
        return;
    }

    switch (msg->cmd) {
        case FLIPPER_CMD_NFC_SCAN_RESULT:
            tag_type = LOKI_FLIPPER_TAG_NFC;
            tag_payload = msg->payload;
            tag_length = msg->length;
            break;
        case FLIPPER_CMD_RFID_SCAN_RESULT:
            tag_type = LOKI_FLIPPER_TAG_RFID;
            tag_payload = msg->payload;
            tag_length = msg->length;
            break;
        case FLIPPER_CMD_SEND_DATA:
            if (msg->payload != NULL && msg->length >= 2U &&
                (msg->payload[0] == LOKI_FLIPPER_TAG_NFC || msg->payload[0] == LOKI_FLIPPER_TAG_RFID)) {
                tag_type = msg->payload[0];
                tag_payload = &msg->payload[1];
                tag_length = (uint16_t)(msg->length - 1U);
            }
            break;
        default:
            break;
    }

    if (tag_type != 0U && tag_payload != NULL && tag_length > 0U) {
        hal_status_t status = persist_flipper_tag_to_eeprom(tag_type, tag_payload, tag_length);
        if (status == HAL_OK) {
            LOG_INFO("Stored %s capture in EEPROM (%u bytes)",
                     flipper_tag_name(tag_type),
                     (unsigned int)tag_length);
            dragon_ai_apply_reward(&app_state.dragon, 0.45f);
        } else {
            LOG_ERROR("Failed to persist %s capture to EEPROM", flipper_tag_name(tag_type));
        }
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
    if (!app_state.config.web_ui_enabled) {
        LOG_WARN("web_ui_enabled was false; forcing Web UI on to keep device accessible");
        app_state.config.web_ui_enabled = true;
    }
    if (app_state.config.web.bind_address[0] == '\0') {
        copy_text(app_state.config.web.bind_address,
                  sizeof(app_state.config.web.bind_address),
                  "0.0.0.0");
    }
    if (app_state.config.web.port == 0U) {
        app_state.config.web.port = 8080U;
    }

    refresh_gps_state();
    load_known_network_cache();

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

    app_state.credits_balance = 1000.0f;
    if (loki_eeprom_load_credits(&app_state.credits_balance) == HAL_OK) {
        LOG_INFO("Loaded credits from EEPROM: %.2f", app_state.credits_balance);
    } else {
        LOG_WARN("EEPROM credits not set or invalid; seeding 1000.00");
        (void)loki_eeprom_save_credits(app_state.credits_balance);
    }

    play_boot_animation();
    sync_display_ready();

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

    request_flipper_scan_capture();

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
                process_flipper_message(&msg);

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

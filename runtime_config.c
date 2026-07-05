#include "runtime_config.h"

#include "board_config.h"
#include "board_profile.h"
#include "log.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define LOKI_CONFIG_LINE_MAX 256

static void loki_strncpy(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static char *trim(char *text)
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

static int parse_bool(const char *value, uint8_t *out)
{
    if (value == NULL || out == NULL) {
        return 0;
    }

    if (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 || strcasecmp(value, "yes") == 0) {
        *out = 1;
        return 1;
    }

    if (strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 || strcasecmp(value, "no") == 0) {
        *out = 0;
        return 1;
    }

    return 0;
}

static int parse_long_checked(const char *value, long *out)
{
    char *end = NULL;
    const char *tail;
    long n;

    if (value == NULL || out == NULL) {
        return 0;
    }

    errno = 0;
    n = strtol(value, &end, 10);
    if (errno != 0 || end == value) {
        return 0;
    }

    tail = end;
    while (*tail != '\0' && isspace((unsigned char)*tail)) {
        tail++;
    }

    if (*tail != '\0') {
        return 0;
    }

    *out = n;
    return 1;
}

static void set_string_field(const char *section, const char *key, const char *value, loki_runtime_config_t *config)
{
    if (strcmp(section, "device") == 0 && strcmp(key, "name") == 0) {
        loki_strncpy(config->device.name, sizeof(config->device.name), value);
    } else if (strcmp(section, "device") == 0 && strcmp(key, "species") == 0) {
        loki_strncpy(config->device.species, sizeof(config->device.species), value);
    } else if (strcmp(section, "device") == 0 && strcmp(key, "stage") == 0) {
        loki_strncpy(config->device.stage, sizeof(config->device.stage), value);
    } else if (strcmp(section, "device") == 0 && strcmp(key, "theme") == 0) {
        loki_strncpy(config->device.theme, sizeof(config->device.theme), value);
    } else if (strcmp(section, "device") == 0 && strcmp(key, "board_profile") == 0) {
        loki_strncpy(config->device.board_profile, sizeof(config->device.board_profile), value);
    } else if (strcmp(section, "personality") == 0 && strcmp(key, "base_mood") == 0) {
        loki_strncpy(config->personality.base_mood, sizeof(config->personality.base_mood), value);
    } else if (strcmp(section, "ui") == 0 && strcmp(key, "idle_animation") == 0) {
        loki_strncpy(config->ui.idle_animation, sizeof(config->ui.idle_animation), value);
    } else if (strcmp(section, "behavior") == 0 && strcmp(key, "evolution_speed") == 0) {
        loki_strncpy(config->behavior.evolution_speed, sizeof(config->behavior.evolution_speed), value);
    } else if (strcmp(section, "logging") == 0 && strcmp(key, "level") == 0) {
        loki_strncpy(config->logging.level, sizeof(config->logging.level), value);
    } else if (strcmp(section, "paths") == 0 && strcmp(key, "data_dir") == 0) {
        loki_strncpy(config->paths.data_dir, sizeof(config->paths.data_dir), value);
    } else if (strcmp(section, "paths") == 0 && strcmp(key, "log_dir") == 0) {
        loki_strncpy(config->paths.log_dir, sizeof(config->paths.log_dir), value);
    }
}

static void set_numeric_or_bool_field(const char *section, const char *key, const char *value, loki_runtime_config_t *config)
{
    uint8_t bool_value = 0;
    long n = 0;
    int has_number = parse_long_checked(value, &n);

    if (strcmp(section, "device") == 0 && strcmp(key, "first_boot") == 0) {
        if (parse_bool(value, &bool_value)) {
            config->device.first_boot = bool_value;
        }
    } else if (strcmp(section, "personality") == 0 && strcmp(key, "mood_enabled") == 0) {
        if (parse_bool(value, &bool_value)) {
            config->personality.mood_enabled = bool_value;
        }
    } else if (strcmp(section, "personality") == 0 && strcmp(key, "mood_decay_rate") == 0 && has_number) {
        config->personality.mood_decay_rate = (uint8_t)n;
    } else if (strcmp(section, "personality") == 0 && strcmp(key, "friendliness") == 0 && has_number) {
        config->personality.friendliness = (uint8_t)n;
    } else if (strcmp(section, "personality") == 0 && strcmp(key, "mischief") == 0 && has_number) {
        config->personality.mischief = (uint8_t)n;
    } else if (strcmp(section, "personality") == 0 && strcmp(key, "energy") == 0 && has_number) {
        config->personality.energy = (uint8_t)n;
    } else if (strcmp(section, "ui") == 0 && strcmp(key, "brightness") == 0 && has_number) {
        config->ui.brightness = (uint8_t)n;
    } else if (strcmp(section, "ui") == 0 && strcmp(key, "rotation") == 0 && has_number) {
        config->ui.rotation = (uint8_t)n;
    } else if (strcmp(section, "ui") == 0 && strcmp(key, "show_stats") == 0) {
        if (parse_bool(value, &bool_value)) {
            config->ui.show_stats = bool_value;
        }
    } else if (strcmp(section, "ui") == 0 && strcmp(key, "show_face") == 0) {
        if (parse_bool(value, &bool_value)) {
            config->ui.show_face = bool_value;
        }
    } else if (strcmp(section, "behavior") == 0 && strcmp(key, "auto_evolve") == 0) {
        if (parse_bool(value, &bool_value)) {
            config->behavior.auto_evolve = bool_value;
        }
    } else if (strcmp(section, "behavior") == 0 && strcmp(key, "sleep_enabled") == 0) {
        if (parse_bool(value, &bool_value)) {
            config->behavior.sleep_enabled = bool_value;
        }
    } else if (strcmp(section, "behavior") == 0 && strcmp(key, "sound_enabled") == 0) {
        if (parse_bool(value, &bool_value)) {
            config->behavior.sound_enabled = bool_value;
        }
    } else if (strcmp(section, "behavior") == 0 && strcmp(key, "interaction_timeout_sec") == 0 && has_number) {
        config->behavior.interaction_timeout_sec = (uint16_t)n;
    } else if (strcmp(section, "credits") == 0 && strcmp(key, "credits_enabled") == 0) {
        if (parse_bool(value, &bool_value)) {
            config->credits.credits_enabled = bool_value;
        }
    } else if (strcmp(section, "credits") == 0 && strcmp(key, "starting_balance") == 0 && has_number) {
        config->credits.starting_balance = (uint32_t)n;
    } else if (strcmp(section, "credits") == 0 && strcmp(key, "allow_write") == 0) {
        if (parse_bool(value, &bool_value)) {
            config->credits.allow_write = bool_value;
        }
    } else if (strcmp(section, "credits") == 0 && strcmp(key, "allow_spend") == 0) {
        if (parse_bool(value, &bool_value)) {
            config->credits.allow_spend = bool_value;
        }
    } else if (strcmp(section, "logging") == 0 && strcmp(key, "error_logging") == 0) {
        if (parse_bool(value, &bool_value)) {
            config->logging.error_logging = bool_value;
        }
    } else if (strcmp(section, "hardware_overrides") == 0 && strcmp(key, "spi_freq_tft") == 0 && has_number) {
        config->hardware_overrides.spi_freq_tft = (uint32_t)n;
    } else if (strcmp(section, "hardware_overrides") == 0 && strcmp(key, "spi_freq_flash") == 0 && has_number) {
        config->hardware_overrides.spi_freq_flash = (uint32_t)n;
    } else if (strcmp(section, "hardware_overrides") == 0 && strcmp(key, "i2c_freq") == 0 && has_number) {
        config->hardware_overrides.i2c_freq = (uint32_t)n;
    } else if (strcmp(section, "hardware_overrides") == 0 && strcmp(key, "uart_baud") == 0 && has_number) {
        config->hardware_overrides.uart_baud = (uint32_t)n;
    }
}

static hal_status_t ensure_parent_dir(const char *path)
{
    char parent[256];
    char *p;
    char *slash;

    if (path == NULL) {
        return HAL_INVALID_PARAM;
    }

    loki_strncpy(parent, sizeof(parent), path);
    slash = strrchr(parent, '/');

    if (slash == NULL || slash == parent) {
        return HAL_OK;
    }

    *slash = '\0';

    for (p = parent + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(parent, 0750) != 0 && errno != EEXIST) {
                return HAL_ERROR;
            }
            *p = '/';
        }
    }

    if (mkdir(parent, 0750) != 0 && errno != EEXIST) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

void loki_config_set_defaults(loki_runtime_config_t *config)
{
    const loki_board_profile_t *profile = loki_board_profile_default();

    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));

    loki_strncpy(config->device.name, sizeof(config->device.name), "Loki");
    loki_strncpy(config->device.species, sizeof(config->device.species), "dragon");
    loki_strncpy(config->device.stage, sizeof(config->device.stage), "egg");
    loki_strncpy(config->device.theme, sizeof(config->device.theme), "default");
    loki_strncpy(config->device.board_profile, sizeof(config->device.board_profile), profile->id);
    config->device.first_boot = 1;

    config->personality.mood_enabled = 1;
    loki_strncpy(config->personality.base_mood, sizeof(config->personality.base_mood), "curious");
    config->personality.mood_decay_rate = 5;
    config->personality.friendliness = 80;
    config->personality.mischief = 40;
    config->personality.energy = 70;

    config->ui.brightness = BOARD_DEFAULT_TFT_BRIGHTNESS;
    config->ui.rotation = BOARD_DEFAULT_TFT_ROTATION;
    config->ui.show_stats = 1;
    config->ui.show_face = 1;
    loki_strncpy(config->ui.idle_animation, sizeof(config->ui.idle_animation), "breathe");

    config->behavior.auto_evolve = 1;
    loki_strncpy(config->behavior.evolution_speed, sizeof(config->behavior.evolution_speed), "normal");
    config->behavior.sleep_enabled = 1;
    config->behavior.sound_enabled = 0;
    config->behavior.interaction_timeout_sec = 60;

    config->credits.credits_enabled = 1;
    config->credits.starting_balance = 100;
    config->credits.allow_write = 1;
    config->credits.allow_spend = 1;

    loki_strncpy(config->logging.level, sizeof(config->logging.level), "info");
    config->logging.error_logging = ENABLE_ERROR_LOGGING ? 1 : 0;

    config->hardware_overrides.spi_freq_tft = profile->tft_spi_freq_hz;
    config->hardware_overrides.spi_freq_flash = profile->flash_spi_freq_hz;
    config->hardware_overrides.i2c_freq = profile->eeprom_i2c_freq_hz;
    config->hardware_overrides.uart_baud = profile->uart_baud_rate;

    loki_strncpy(config->paths.data_dir, sizeof(config->paths.data_dir), "/var/lib/loki");
    loki_strncpy(config->paths.log_dir, sizeof(config->paths.log_dir), "/var/log/loki");
}

hal_status_t loki_config_validate(loki_runtime_config_t *config)
{
    if (config == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (config->ui.brightness > 100) {
        config->ui.brightness = 100;
    }

    if (config->ui.rotation > 3) {
        config->ui.rotation = BOARD_DEFAULT_TFT_ROTATION;
    }

    if (config->personality.friendliness > 100) {
        config->personality.friendliness = 100;
    }

    if (config->personality.mischief > 100) {
        config->personality.mischief = 100;
    }

    if (config->personality.energy > 100) {
        config->personality.energy = 100;
    }

    if (config->behavior.interaction_timeout_sec == 0) {
        config->behavior.interaction_timeout_sec = 60;
    }

    if (!loki_board_profile_is_supported(config->device.board_profile)) {
        const loki_board_profile_t *profile = loki_board_profile_default();
        loki_strncpy(config->device.board_profile, sizeof(config->device.board_profile), profile->id);
    }

    if (config->logging.level[0] == '\0') {
        loki_strncpy(config->logging.level, sizeof(config->logging.level), "info");
    }

    if (config->paths.data_dir[0] == '\0') {
        loki_strncpy(config->paths.data_dir, sizeof(config->paths.data_dir), "/var/lib/loki");
    }

    if (config->paths.log_dir[0] == '\0') {
        loki_strncpy(config->paths.log_dir, sizeof(config->paths.log_dir), "/var/log/loki");
    }

    return HAL_OK;
}

hal_status_t loki_config_load(const char *path, loki_runtime_config_t *config)
{
    FILE *fp;
    char line[LOKI_CONFIG_LINE_MAX];
    char current_section[64] = "";

    if (path == NULL || config == NULL) {
        return HAL_INVALID_PARAM;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        return HAL_ERROR;
    }

    loki_config_set_defaults(config);

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *content = trim(line);

        if (content[0] == '\0' || content[0] == '#' || content[0] == ';') {
            continue;
        }

        if (content[0] == '[') {
            char *end = strchr(content, ']');
            if (end != NULL) {
                *end = '\0';
                loki_strncpy(current_section, sizeof(current_section), content + 1);
            }
            continue;
        }

        {
            char *eq = strchr(content, '=');
            if (eq == NULL) {
                continue;
            }

            *eq = '\0';
            char *key = trim(content);
            char *value = trim(eq + 1);

            set_string_field(current_section, key, value, config);
            set_numeric_or_bool_field(current_section, key, value, config);
        }
    }

    fclose(fp);
    return loki_config_validate(config);
}

hal_status_t loki_config_save(const char *path, const loki_runtime_config_t *config)
{
    FILE *fp;

    if (path == NULL || config == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (ensure_parent_dir(path) != HAL_OK) {
        return HAL_ERROR;
    }

    fp = fopen(path, "w");
    if (fp == NULL) {
        return HAL_ERROR;
    }

    fprintf(fp, "# Loki master runtime config\n");
    fprintf(fp, "# Generated by Loki setup/config manager\n\n");

    fprintf(fp, "[device]\n");
    fprintf(fp, "name=%s\n", config->device.name);
    fprintf(fp, "species=%s\n", config->device.species);
    fprintf(fp, "stage=%s\n", config->device.stage);
    fprintf(fp, "theme=%s\n", config->device.theme);
    fprintf(fp, "board_profile=%s\n", config->device.board_profile);
    fprintf(fp, "first_boot=%s\n\n", config->device.first_boot ? "true" : "false");

    fprintf(fp, "[personality]\n");
    fprintf(fp, "mood_enabled=%s\n", config->personality.mood_enabled ? "true" : "false");
    fprintf(fp, "base_mood=%s\n", config->personality.base_mood);
    fprintf(fp, "mood_decay_rate=%u\n", config->personality.mood_decay_rate);
    fprintf(fp, "friendliness=%u\n", config->personality.friendliness);
    fprintf(fp, "mischief=%u\n", config->personality.mischief);
    fprintf(fp, "energy=%u\n\n", config->personality.energy);

    fprintf(fp, "[ui]\n");
    fprintf(fp, "brightness=%u\n", config->ui.brightness);
    fprintf(fp, "rotation=%u\n", config->ui.rotation);
    fprintf(fp, "show_stats=%s\n", config->ui.show_stats ? "true" : "false");
    fprintf(fp, "show_face=%s\n", config->ui.show_face ? "true" : "false");
    fprintf(fp, "idle_animation=%s\n\n", config->ui.idle_animation);

    fprintf(fp, "[behavior]\n");
    fprintf(fp, "auto_evolve=%s\n", config->behavior.auto_evolve ? "true" : "false");
    fprintf(fp, "evolution_speed=%s\n", config->behavior.evolution_speed);
    fprintf(fp, "sleep_enabled=%s\n", config->behavior.sleep_enabled ? "true" : "false");
    fprintf(fp, "sound_enabled=%s\n", config->behavior.sound_enabled ? "true" : "false");
    fprintf(fp, "interaction_timeout_sec=%u\n\n", config->behavior.interaction_timeout_sec);

    fprintf(fp, "[credits]\n");
    fprintf(fp, "credits_enabled=%s\n", config->credits.credits_enabled ? "true" : "false");
    fprintf(fp, "starting_balance=%u\n", config->credits.starting_balance);
    fprintf(fp, "allow_write=%s\n", config->credits.allow_write ? "true" : "false");
    fprintf(fp, "allow_spend=%s\n\n", config->credits.allow_spend ? "true" : "false");

    fprintf(fp, "[logging]\n");
    fprintf(fp, "level=%s\n", config->logging.level);
    fprintf(fp, "error_logging=%s\n\n", config->logging.error_logging ? "true" : "false");

    fprintf(fp, "[hardware_overrides]\n");
    fprintf(fp, "spi_freq_tft=%u\n", config->hardware_overrides.spi_freq_tft);
    fprintf(fp, "spi_freq_flash=%u\n", config->hardware_overrides.spi_freq_flash);
    fprintf(fp, "i2c_freq=%u\n", config->hardware_overrides.i2c_freq);
    fprintf(fp, "uart_baud=%u\n\n", config->hardware_overrides.uart_baud);

    fprintf(fp, "[paths]\n");
    fprintf(fp, "data_dir=%s\n", config->paths.data_dir);
    fprintf(fp, "log_dir=%s\n", config->paths.log_dir);

    fclose(fp);
    return HAL_OK;
}

hal_status_t loki_config_load_or_create(const char *path, loki_runtime_config_t *config, uint8_t *created_default)
{
    hal_status_t status;

    if (path == NULL || config == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (created_default != NULL) {
        *created_default = 0;
    }

    status = loki_config_load(path, config);
    if (status == HAL_OK) {
        return HAL_OK;
    }

    loki_config_set_defaults(config);
    status = loki_config_save(path, config);

    if (status == HAL_OK && created_default != NULL) {
        *created_default = 1;
    }

    if (status == HAL_OK) {
        LOG_INFO("Created default runtime config: %s", path);
    }

    return status;
}

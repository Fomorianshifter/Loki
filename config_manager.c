/**
 * @file config_manager.c
 * @brief Central runtime configuration manager implementation.
 *
 * Boot sequence:
 *   1. Load compile-time defaults
 *   2. Initialize EEPROM and read bootstrap metadata
 *   3. Initialize SD card
 *   4. Load loki_config.ini from SD card
 *   5. Validate and apply configuration
 *   6. Initialize plugins and programs
 *   7. Fall back to defaults if any step fails
 */

#include "config_manager.h"
#include "eeprom_layout.h"
#include "plugin_registry.h"
#include "program_registry.h"
#include "log.h"

/* Driver includes — adjust paths if repo structure changes */
#include "eeprom_driver.h"
#include "sdcard_driver.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* ===== MODULE STATE ===== */

static loki_config_t   g_config;
static uint8_t         g_initialized    = 0;
static eeprom_bootstrap_t g_bootstrap;

/* ===== INTERNAL: COMPILE-TIME DEFAULTS ===== */

static void apply_defaults(loki_config_t *cfg)
{
    memset(cfg, 0, sizeof(loki_config_t));

    strncpy(cfg->startup_mode, DEFAULT_STARTUP_MODE, sizeof(cfg->startup_mode) - 1);
    strncpy(cfg->log_level,    DEFAULT_LOG_LEVEL,    sizeof(cfg->log_level)    - 1);
    cfg->safe_mode = DEFAULT_SAFE_MODE;

    cfg->ai.enabled    = DEFAULT_AI_ENABLED;
    strncpy(cfg->ai.model, DEFAULT_AI_MODEL, sizeof(cfg->ai.model) - 1);
    cfg->ai.max_tokens = DEFAULT_AI_MAX_TOKENS;

    cfg->storage.sd_config_enabled   = DEFAULT_SD_CONFIG_ENABLED;
    cfg->storage.fallback_to_defaults = DEFAULT_FALLBACK_TO_DEFAULTS;
    strncpy(cfg->storage.sd_mount_point, DEFAULT_SD_MOUNT_POINT,
            sizeof(cfg->storage.sd_mount_point) - 1);

    cfg->display.brightness = DEFAULT_BRIGHTNESS;
    cfg->display.rotation   = DEFAULT_ROTATION;

    cfg->flipper.enabled   = DEFAULT_FLIPPER_ENABLED;
    cfg->flipper.baud_rate = DEFAULT_FLIPPER_BAUD;

    cfg->mood.enabled = DEFAULT_MOOD_ENABLED;
    strncpy(cfg->mood.default_mood, DEFAULT_MOOD, sizeof(cfg->mood.default_mood) - 1);

    cfg->safety.safe_mode_on_boot  = DEFAULT_SAFE_MODE_ON_BOOT;
    cfg->safety.recovery_enabled   = DEFAULT_RECOVERY_ENABLED;
    cfg->safety.watchdog_timeout_s = DEFAULT_WATCHDOG_TIMEOUT_S;

    cfg->plugin_count  = 0;
    cfg->program_count = 0;
    cfg->source        = CFG_SOURCE_DEFAULTS;
}

/* ===== INTERNAL: EEPROM BOOTSTRAP ===== */

/**
 * CRC-16/CCITT (poly 0x1021, init 0xFFFF).
 */
uint16_t eeprom_crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (uint16_t)(crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void eeprom_bootstrap_defaults(eeprom_bootstrap_t *bs)
{
    memset(bs, 0, sizeof(eeprom_bootstrap_t));
    bs->magic             = EEPROM_MAGIC_VALUE;
    bs->schema_ver        = EEPROM_SCHEMA_VERSION;
    bs->config_ver        = 1;
    bs->boot_count        = 0;
    bs->safe_mode_flag    = 0;
    bs->sd_config_enabled = 1;
    bs->brightness        = DEFAULT_BRIGHTNESS;
    bs->startup_mode      = EEPROM_MODE_NORMAL;
}

hal_status_t eeprom_bootstrap_read(eeprom_bootstrap_t *bs)
{
    if (bs == NULL) {
        return HAL_INVALID_PARAM;
    }

    uint8_t raw[EEPROM_BOOTSTRAP_SIZE];
    hal_status_t st = eeprom_read(EEPROM_OFF_MAGIC, raw, EEPROM_BOOTSTRAP_SIZE);
    if (st != HAL_OK) {
        LOG_ERROR("EEPROM bootstrap read failed (status=%d)", st);
        eeprom_bootstrap_defaults(bs);
        return HAL_ERROR;
    }

    /* Decode little-endian fields */
    bs->magic             = (uint16_t)(raw[0] | ((uint16_t)raw[1] << 8));
    bs->schema_ver        = raw[2];
    bs->config_ver        = raw[3];
    bs->boot_count        = (uint16_t)(raw[4] | ((uint16_t)raw[5] << 8));
    bs->safe_mode_flag    = raw[6];
    bs->sd_config_enabled = raw[7];
    bs->brightness        = raw[8];
    bs->startup_mode      = raw[9];
    memcpy(bs->reserved, &raw[10], 4);
    bs->crc16             = (uint16_t)(raw[14] | ((uint16_t)raw[15] << 8));

    /* Validate magic */
    if (bs->magic != EEPROM_MAGIC_VALUE) {
        LOG_WARN("EEPROM magic mismatch (got 0x%04X, expected 0x%04X) — first boot?",
                 bs->magic, EEPROM_MAGIC_VALUE);
        eeprom_bootstrap_defaults(bs);
        return HAL_ERROR;
    }

    /* Validate schema version */
    if (bs->schema_ver != EEPROM_SCHEMA_VERSION) {
        LOG_WARN("EEPROM schema version mismatch (got %d, expected %d)",
                 bs->schema_ver, EEPROM_SCHEMA_VERSION);
        eeprom_bootstrap_defaults(bs);
        return HAL_ERROR;
    }

    /* Validate CRC */
    uint16_t expected_crc = eeprom_crc16(raw, EEPROM_CRC_DATA_LEN);
    if (bs->crc16 != expected_crc) {
        LOG_WARN("EEPROM CRC mismatch (got 0x%04X, expected 0x%04X)",
                 bs->crc16, expected_crc);
        eeprom_bootstrap_defaults(bs);
        return HAL_ERROR;
    }

    return HAL_OK;
}

hal_status_t eeprom_bootstrap_write(const eeprom_bootstrap_t *bs)
{
    if (bs == NULL) {
        return HAL_INVALID_PARAM;
    }

    uint8_t raw[EEPROM_BOOTSTRAP_SIZE];
    memset(raw, 0, sizeof(raw));

    /* Encode little-endian fields */
    raw[0]  = (uint8_t)(bs->magic & 0xFF);
    raw[1]  = (uint8_t)(bs->magic >> 8);
    raw[2]  = bs->schema_ver;
    raw[3]  = bs->config_ver;
    raw[4]  = (uint8_t)(bs->boot_count & 0xFF);
    raw[5]  = (uint8_t)(bs->boot_count >> 8);
    raw[6]  = bs->safe_mode_flag;
    raw[7]  = bs->sd_config_enabled;
    raw[8]  = bs->brightness;
    raw[9]  = bs->startup_mode;
    memcpy(&raw[10], bs->reserved, 4);

    /* Compute and store CRC over first 14 bytes */
    uint16_t crc = eeprom_crc16(raw, EEPROM_CRC_DATA_LEN);
    raw[14] = (uint8_t)(crc & 0xFF);
    raw[15] = (uint8_t)(crc >> 8);

    return eeprom_write(EEPROM_OFF_MAGIC, raw, EEPROM_BOOTSTRAP_SIZE);
}

/* ===== INTERNAL: INI PARSER ===== */

/**
 * Trim leading and trailing whitespace in-place.
 */
static char *trim(char *s)
{
    if (s == NULL) return s;
    /* leading */
    while (*s && isspace((unsigned char)*s)) s++;
    /* trailing */
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';
    return s;
}

/**
 * Parse a boolean value string ("true"/"1"/"yes" → 1, anything else → 0).
 */
static uint8_t parse_bool(const char *val)
{
    if (val == NULL) return 0;
    return (strcmp(val, "true") == 0  ||
            strcmp(val, "1")    == 0  ||
            strcmp(val, "yes")  == 0) ? 1 : 0;
}

/**
 * Apply a single key=value pair from loki_config.ini to *cfg.
 */
static void apply_kv(loki_config_t *cfg,
                     const char    *section,
                     const char    *key,
                     const char    *val)
{
    /* ---- [system] ---- */
    if (strcmp(section, "system") == 0) {
        if (strcmp(key, "startup_mode") == 0) {
            strncpy(cfg->startup_mode, val, sizeof(cfg->startup_mode) - 1);
        } else if (strcmp(key, "log_level") == 0) {
            strncpy(cfg->log_level, val, sizeof(cfg->log_level) - 1);
        } else if (strcmp(key, "safe_mode") == 0) {
            cfg->safe_mode = parse_bool(val);
        }
        return;
    }

    /* ---- [ai] ---- */
    if (strcmp(section, "ai") == 0) {
        if (strcmp(key, "enabled") == 0) {
            cfg->ai.enabled = parse_bool(val);
        } else if (strcmp(key, "model") == 0) {
            strncpy(cfg->ai.model, val, sizeof(cfg->ai.model) - 1);
        } else if (strcmp(key, "max_tokens") == 0) {
            cfg->ai.max_tokens = (uint16_t)atoi(val);
        }
        return;
    }

    /* ---- [plugins] ---- */
    if (strcmp(section, "plugins") == 0) {
        /* Keys: plugin<N>=<name>, plugin<N>_enabled=<bool>, plugin<N>_order=<int> */
        if (strncmp(key, "plugin", 6) == 0) {
            const char *rest = key + 6;
            char *underscore = strchr(rest, '_');

            if (underscore == NULL) {
                /* plugin<N>=<name> — declare a new plugin entry */
                uint8_t idx = (uint8_t)atoi(rest);
                if (idx < MAX_PLUGINS) {
                    if (idx >= cfg->plugin_count) {
                        cfg->plugin_count = (uint8_t)(idx + 1);
                    }
                    strncpy(cfg->plugins[idx].name, val,
                            sizeof(cfg->plugins[idx].name) - 1);
                    cfg->plugins[idx].enabled = 1; /* default enabled */
                }
            } else {
                /* plugin<N>_enabled or plugin<N>_order */
                uint8_t idx = (uint8_t)atoi(rest);
                if (idx < MAX_PLUGINS) {
                    if (strcmp(underscore + 1, "enabled") == 0) {
                        cfg->plugins[idx].enabled = parse_bool(val);
                    } else if (strcmp(underscore + 1, "order") == 0) {
                        cfg->plugins[idx].order = (uint8_t)atoi(val);
                    }
                }
            }
        }
        return;
    }

    /* ---- [programs] ---- */
    if (strcmp(section, "programs") == 0) {
        if (strncmp(key, "program", 7) == 0) {
            const char *rest = key + 7;
            char *underscore = strchr(rest, '_');

            if (underscore == NULL) {
                /* program<N>=<name> */
                uint8_t idx = (uint8_t)atoi(rest);
                if (idx < MAX_PROGRAMS) {
                    if (idx >= cfg->program_count) {
                        cfg->program_count = (uint8_t)(idx + 1);
                    }
                    strncpy(cfg->programs[idx].name, val,
                            sizeof(cfg->programs[idx].name) - 1);
                    cfg->programs[idx].enabled = 1;
                }
            } else {
                uint8_t idx = (uint8_t)atoi(rest);
                if (idx < MAX_PROGRAMS) {
                    const char *field = underscore + 1;
                    if (strcmp(field, "path") == 0) {
                        strncpy(cfg->programs[idx].path, val,
                                sizeof(cfg->programs[idx].path) - 1);
                    } else if (strcmp(field, "enabled") == 0) {
                        cfg->programs[idx].enabled = parse_bool(val);
                    } else if (strcmp(field, "auto_start") == 0) {
                        cfg->programs[idx].auto_start = parse_bool(val);
                    }
                }
            }
        }
        return;
    }

    /* ---- [storage] ---- */
    if (strcmp(section, "storage") == 0) {
        if (strcmp(key, "sd_config_enabled") == 0) {
            cfg->storage.sd_config_enabled = parse_bool(val);
        } else if (strcmp(key, "sd_mount_point") == 0) {
            strncpy(cfg->storage.sd_mount_point, val,
                    sizeof(cfg->storage.sd_mount_point) - 1);
        } else if (strcmp(key, "fallback_to_defaults") == 0) {
            cfg->storage.fallback_to_defaults = parse_bool(val);
        }
        return;
    }

    /* ---- [hardware] ---- */
    /* Hardware section keys are informational; runtime params are in [display] etc. */
    (void)section; (void)key; (void)val;

    /* ---- [display] ---- */
    if (strcmp(section, "display") == 0) {
        if (strcmp(key, "brightness") == 0) {
            cfg->display.brightness = (uint8_t)atoi(val);
        } else if (strcmp(key, "rotation") == 0) {
            cfg->display.rotation = (uint8_t)atoi(val);
        }
        return;
    }

    /* ---- [flipper] ---- */
    if (strcmp(section, "flipper") == 0) {
        if (strcmp(key, "enabled") == 0) {
            cfg->flipper.enabled = parse_bool(val);
        } else if (strcmp(key, "baud_rate") == 0) {
            cfg->flipper.baud_rate = (uint32_t)atoi(val);
        }
        return;
    }

    /* ---- [mood] ---- */
    if (strcmp(section, "mood") == 0) {
        if (strcmp(key, "enabled") == 0) {
            cfg->mood.enabled = parse_bool(val);
        } else if (strcmp(key, "default_mood") == 0) {
            strncpy(cfg->mood.default_mood, val,
                    sizeof(cfg->mood.default_mood) - 1);
        }
        return;
    }

    /* ---- [safety] ---- */
    if (strcmp(section, "safety") == 0) {
        if (strcmp(key, "safe_mode_on_boot") == 0) {
            cfg->safety.safe_mode_on_boot = parse_bool(val);
        } else if (strcmp(key, "recovery_enabled") == 0) {
            cfg->safety.recovery_enabled = parse_bool(val);
        } else if (strcmp(key, "watchdog_timeout_s") == 0) {
            cfg->safety.watchdog_timeout_s = (uint16_t)atoi(val);
        }
        return;
    }
}

/**
 * Parse loki_config.ini from a memory buffer into *cfg.
 *
 * Format: INI-style — [section] headers, key=value lines.
 * Lines starting with '#' or ';' are comments.
 * Inline comments are NOT supported.
 *
 * @param[inout] cfg     Config struct already populated with defaults
 * @param[in]   buf     NUL-terminated config file content
 * @return HAL_OK always (unknown keys are silently ignored)
 */
static hal_status_t parse_ini(loki_config_t *cfg, char *buf)
{
    char section[CONFIG_MAX_SECTION_LEN] = "";
    char *line = buf;

    while (line && *line) {
        /* Find end of line */
        char *eol = strchr(line, '\n');
        if (eol) *eol = '\0';

        char *trimmed = trim(line);

        /* Advance past this line */
        line = eol ? eol + 1 : line + strlen(line);

        if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        /* Section header */
        if (trimmed[0] == '[') {
            char *close = strchr(trimmed, ']');
            if (close) {
                *close = '\0';
                strncpy(section, trimmed + 1, CONFIG_MAX_SECTION_LEN - 1);
                section[CONFIG_MAX_SECTION_LEN - 1] = '\0';
            }
            continue;
        }

        /* key=value */
        char *eq = strchr(trimmed, '=');
        if (eq == NULL) continue;

        *eq = '\0';
        char *key = trim(trimmed);
        char *val = trim(eq + 1);

        if (key[0] == '\0') continue;

        apply_kv(cfg, section, key, val);
    }

    return HAL_OK;
}

/* ===== INTERNAL: SD CONFIG LOADER ===== */

/**
 * Read loki_config.ini from SD card into a heap buffer, then parse it.
 *
 * This is a sector-level implementation that reads raw SD sectors.
 * It assumes the config file starts at sector 2048 (1 MiB offset) as a
 * simple convention — in a production system with a FAT filesystem driver
 * this would use fopen/fread.  The file is expected to fit within
 * CONFIG_FILE_MAX_BYTES.
 *
 * NOTE: Without a full filesystem driver, this function uses a simplified
 * approach.  Replace with a proper FAT read when a FS layer is available.
 */
#define CONFIG_FILE_MAX_BYTES   4096U  /* Max config file size we will read */
#define CONFIG_SD_SECTOR_START  2048U  /* Conventional sector offset */

static hal_status_t load_config_from_sd(loki_config_t *cfg)
{
    /* Allocate buffer for the config file content */
    char *buf = (char *)malloc(CONFIG_FILE_MAX_BYTES + 1);
    if (buf == NULL) {
        LOG_ERROR("Failed to allocate config buffer");
        return HAL_ERROR;
    }
    memset(buf, 0, CONFIG_FILE_MAX_BYTES + 1);

    /* Read sectors covering the config file (512 bytes each) */
    uint16_t sectors = (uint16_t)((CONFIG_FILE_MAX_BYTES + 511) / 512);
    hal_status_t st = sdcard_read_sector(CONFIG_SD_SECTOR_START, sectors,
                                          (uint8_t *)buf);
    if (st != HAL_OK) {
        LOG_WARN("SD config read failed (sector %u, status=%d) — using defaults",
                 CONFIG_SD_SECTOR_START, st);
        free(buf);
        return HAL_ERROR;
    }

    /* Ensure NUL-termination */
    buf[CONFIG_FILE_MAX_BYTES] = '\0';

    /* Sanity check: first character should look like a comment or '[' */
    if (buf[0] != '[' && buf[0] != '#' && buf[0] != ';') {
        LOG_WARN("SD config does not look like an INI file — using defaults");
        free(buf);
        return HAL_ERROR;
    }

    LOG_INFO("Parsing SD config (%u bytes max)...", CONFIG_FILE_MAX_BYTES);
    parse_ini(cfg, buf);

    free(buf);
    cfg->source = CFG_SOURCE_SD;
    return HAL_OK;
}

/* ===== INTERNAL: APPLY CONFIG TO REGISTRIES ===== */

static void sync_plugin_registry(const loki_config_t *cfg)
{
    for (uint8_t i = 0; i < cfg->plugin_count; i++) {
        const cfg_plugin_t *p = &cfg->plugins[i];
        if (p->name[0] == '\0') continue;

        /* The plugin must already be registered by application code.
         * The config only sets enabled/order — it does not create new entries. */
        plugin_registry_set_enabled(p->name, p->enabled);
        plugin_registry_set_order(p->name,   p->order);
    }
}

static void sync_program_registry(const loki_config_t *cfg)
{
    program_registry_clear();
    for (uint8_t i = 0; i < cfg->program_count; i++) {
        const cfg_program_t *p = &cfg->programs[i];
        if (p->name[0] == '\0') continue;
        program_registry_register(p->name, p->path, p->enabled, p->auto_start);
    }
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t config_manager_init(void)
{
    if (g_initialized) {
        LOG_INFO("Config manager already initialized");
        return HAL_OK;
    }

    LOG_INFO("─── Config Manager: starting boot sequence ───");

    /* ── Step 1: compile-time defaults ── */
    LOG_INFO("[1/7] Loading compile-time defaults...");
    apply_defaults(&g_config);
    LOG_INFO("      Defaults loaded (startup_mode=%s, brightness=%d)",
             g_config.startup_mode, g_config.display.brightness);

    /* ── Step 2: init EEPROM ── */
    LOG_INFO("[2/7] Initializing EEPROM...");
    hal_status_t eeprom_ok = eeprom_init();
    if (eeprom_ok != HAL_OK) {
        LOG_WARN("      EEPROM init failed — bootstrap metadata unavailable");
    }

    /* ── Step 3: read EEPROM bootstrap ── */
    LOG_INFO("[3/7] Reading EEPROM bootstrap metadata...");
    uint8_t bootstrap_valid = 0;

    if (eeprom_ok == HAL_OK) {
        if (eeprom_bootstrap_read(&g_bootstrap) == HAL_OK) {
            bootstrap_valid = 1;
            LOG_INFO("      EEPROM bootstrap valid: boot_count=%u  safe_mode=%d  sd_cfg=%d",
                     g_bootstrap.boot_count,
                     g_bootstrap.safe_mode_flag,
                     g_bootstrap.sd_config_enabled);

            /* Increment boot counter and write back */
            if (g_bootstrap.boot_count < 0xFFFF) {
                g_bootstrap.boot_count++;
            }
            eeprom_bootstrap_write(&g_bootstrap);

            /* Override display brightness from EEPROM */
            g_config.display.brightness = g_bootstrap.brightness;
            g_config.source = CFG_SOURCE_EEPROM;

            /* Check safe-mode flag */
            if (g_bootstrap.safe_mode_flag) {
                LOG_WARN("      *** SAFE MODE activated by EEPROM flag ***");
                g_config.safe_mode = 1;
                strncpy(g_config.startup_mode, "safe",
                        sizeof(g_config.startup_mode) - 1);
            }

            /* Override SD-config-enabled from EEPROM */
            g_config.storage.sd_config_enabled = g_bootstrap.sd_config_enabled;
        } else {
            LOG_WARN("      EEPROM bootstrap invalid/first-boot — writing defaults");
            eeprom_bootstrap_defaults(&g_bootstrap);
            g_bootstrap.boot_count = 1;
            eeprom_bootstrap_write(&g_bootstrap);
        }
    }
    (void)bootstrap_valid;

    /* ── Step 4: init SD card ── */
    LOG_INFO("[4/7] Initializing SD card...");
    hal_status_t sd_ok = sdcard_init();
    if (sd_ok != HAL_OK) {
        LOG_WARN("      SD card init failed — cannot load config from SD");
    }

    /* ── Step 5: load config from SD ── */
    if (!g_config.safe_mode && g_config.storage.sd_config_enabled && sd_ok == HAL_OK) {
        LOG_INFO("[5/7] Loading master config from SD card (%s)...",
                 DEFAULT_CONFIG_PATH);
        if (load_config_from_sd(&g_config) == HAL_OK) {
            LOG_INFO("      Config loaded from SD card successfully");
        } else {
            LOG_WARN("      SD config load failed — using defaults/EEPROM values");
        }
    } else {
        if (g_config.safe_mode) {
            LOG_INFO("[5/7] SAFE MODE: skipping SD config load");
        } else if (!g_config.storage.sd_config_enabled) {
            LOG_INFO("[5/7] SD config disabled — using compile-time defaults");
        } else {
            LOG_INFO("[5/7] SD not available — using compile-time defaults");
        }
    }

    /* ── Step 6: validate and apply ── */
    LOG_INFO("[6/7] Validating and applying configuration...");

    /* Clamp brightness */
    if (g_config.display.brightness > 100) {
        LOG_WARN("      brightness %d > 100, clamping to 100",
                 g_config.display.brightness);
        g_config.display.brightness = 100;
    }

    /* Validate startup_mode */
    if (strcmp(g_config.startup_mode, "normal") != 0 &&
        strcmp(g_config.startup_mode, "safe")   != 0 &&
        strcmp(g_config.startup_mode, "demo")   != 0) {
        LOG_WARN("      Unknown startup_mode '%s', reverting to 'normal'",
                 g_config.startup_mode);
        strncpy(g_config.startup_mode, "normal", sizeof(g_config.startup_mode) - 1);
    }

    /* Sync plugin/program registries */
    sync_plugin_registry(&g_config);
    sync_program_registry(&g_config);

    LOG_INFO("      Configuration applied:");
    LOG_INFO("        startup_mode  : %s", g_config.startup_mode);
    LOG_INFO("        log_level     : %s", g_config.log_level);
    LOG_INFO("        safe_mode     : %d", g_config.safe_mode);
    LOG_INFO("        ai.enabled    : %d", g_config.ai.enabled);
    LOG_INFO("        display.bright: %d%%", g_config.display.brightness);
    LOG_INFO("        flipper.enable: %d", g_config.flipper.enabled);
    LOG_INFO("        plugins       : %d configured", g_config.plugin_count);
    LOG_INFO("        programs      : %d configured", g_config.program_count);

    /* ── Step 7: start plugins / programs ── */
    LOG_INFO("[7/7] Starting plugins and programs...");

    if (!g_config.safe_mode) {
        hal_status_t plugin_st = plugin_registry_init_all();
        if (plugin_st != HAL_OK) {
            LOG_WARN("      One or more plugins failed to initialize");
        }
        program_registry_print_all();
        /* auto_start programs are launched by the main loop / scheduler */
    } else {
        LOG_INFO("      SAFE MODE: plugins and programs not started");
    }

    g_initialized = 1;

    LOG_INFO("─── Config Manager: boot sequence complete ───");
    LOG_INFO("    Config source: %s",
             g_config.source == CFG_SOURCE_SD       ? "SD card" :
             g_config.source == CFG_SOURCE_EEPROM   ? "EEPROM + defaults" :
                                                       "compile-time defaults");

    return HAL_OK;
}

const loki_config_t *config_manager_get(void)
{
    if (!g_initialized) {
        return NULL;
    }
    return &g_config;
}

config_source_t config_manager_source(void)
{
    return g_config.source;
}

hal_status_t config_manager_reload(void)
{
    LOG_INFO("Config manager: hot-reload requested");
    apply_defaults(&g_config);

    hal_status_t st = load_config_from_sd(&g_config);
    if (st == HAL_OK) {
        sync_plugin_registry(&g_config);
        sync_program_registry(&g_config);
        LOG_INFO("Config manager: hot-reload complete");
    } else {
        LOG_WARN("Config manager: hot-reload failed, keeping previous config");
    }
    return st;
}

hal_status_t config_manager_save_bootstrap(void)
{
    g_bootstrap.brightness        = g_config.display.brightness;
    g_bootstrap.sd_config_enabled = g_config.storage.sd_config_enabled;
    g_bootstrap.safe_mode_flag    = g_config.safe_mode;

    if (strcmp(g_config.startup_mode, "safe") == 0) {
        g_bootstrap.startup_mode = EEPROM_MODE_SAFE;
    } else if (strcmp(g_config.startup_mode, "demo") == 0) {
        g_bootstrap.startup_mode = EEPROM_MODE_DEMO;
    } else {
        g_bootstrap.startup_mode = EEPROM_MODE_NORMAL;
    }

    return eeprom_bootstrap_write(&g_bootstrap);
}

void config_manager_deinit(void)
{
    if (!g_initialized) return;

    plugin_registry_deinit_all();
    program_registry_clear();
    plugin_registry_clear();

    g_initialized = 0;
    LOG_INFO("Config manager deinitialized");
}

#include "tool_runner.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "platform.h"

static void tool_copy_text(char *dest, size_t dest_size, const char *src)
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

#if PLATFORM == PLATFORM_LINUX
static hal_status_t tool_capture_fixed_command(const char *command,
                                               const char *title,
                                               char *output,
                                               size_t output_size)
{
    FILE *pipe;
    size_t used;

    if (command == NULL || title == NULL || output == NULL || output_size == 0U) {
        return HAL_INVALID_PARAM;
    }

    snprintf(output, output_size, "%s\n", title);
    used = strlen(output);

    pipe = popen(command, "r");
    if (pipe == NULL) {
        snprintf(output + used,
                 output_size - used,
                 "Probe command failed to launch: %s\n",
                 command);
        return HAL_ERROR;
    }

    while (used < output_size - 1U && fgets(output + used, (int)(output_size - used), pipe) != NULL) {
        used = strlen(output);
    }

    if (pclose(pipe) != 0 && used < output_size - 1U) {
        snprintf(output + used,
                 output_size - used,
                 "Probe command exited non-zero. Utility may be unavailable.\n");
        used = strlen(output);
    }

    if (used <= strlen(title) + 1U) {
        tool_copy_text(output + used,
                       output_size - used,
                       "No probe output was returned.\n");
    }

    return HAL_OK;
}
#endif

static bool tool_text_matches_charset(const char *text, const char *extra_chars)
{
    size_t index;

    if (text == NULL || text[0] == '\0') {
        return false;
    }

    for (index = 0; text[index] != '\0'; index++) {
        unsigned char character = (unsigned char)text[index];
        if (isalnum(character) || strchr("._:/-", (int)character) != NULL) {
            continue;
        }
        if (extra_chars != NULL && strchr(extra_chars, (int)character) != NULL) {
            continue;
        }
        return false;
    }

    return true;
}

static bool tool_query_get_value(const char *query,
                                 const char *key,
                                 char *dest,
                                 size_t dest_size)
{
    size_t key_length;
    const char *cursor;

    if (query == NULL || key == NULL || dest == NULL || dest_size == 0U) {
        return false;
    }

    key_length = strlen(key);
    cursor = query;
    while (*cursor != '\0') {
        const char *entry_end = strchr(cursor, '&');
        const char *equals = strchr(cursor, '=');
        size_t value_length;

        if (entry_end == NULL) {
            entry_end = cursor + strlen(cursor);
        }
        if (equals != NULL && equals < entry_end && (size_t)(equals - cursor) == key_length &&
            strncmp(cursor, key, key_length) == 0) {
            value_length = (size_t)(entry_end - (equals + 1));
            if (value_length >= dest_size) {
                value_length = dest_size - 1U;
            }
            memcpy(dest, equals + 1, value_length);
            dest[value_length] = '\0';
            return true;
        }

        if (*entry_end == '\0') {
            break;
        }
        cursor = entry_end + 1;
    }

    dest[0] = '\0';
    return false;
}

static bool tool_is_scan_target_valid(const char *target)
{
    return tool_text_matches_charset(target, NULL);
}

static bool tool_is_port_list_valid(const char *ports)
{
    return tool_text_matches_charset(ports, ",");
}

static bool tool_is_scan_mode_valid(const char *mode)
{
    return strcmp(mode, "ping") == 0 || strcmp(mode, "service") == 0 || strcmp(mode, "quick") == 0;
}

#if PLATFORM == PLATFORM_LINUX
static hal_status_t tool_run_external_command(const char *command,
                                              char *output,
                                              size_t output_size)
{
    FILE *pipe;
    size_t used = 0U;

    pipe = popen(command, "r");
    if (pipe == NULL) {
        snprintf(output, output_size, "Failed to launch command.");
        return HAL_ERROR;
    }

    output[0] = '\0';
    while (fgets(output + used, (int)(output_size - used), pipe) != NULL) {
        used = strlen(output);
        if (used >= output_size - 1U) {
            break;
        }
    }

    if (pclose(pipe) != 0 && output[0] == '\0') {
        tool_copy_text(output, output_size, "Command exited non-zero with no output.");
        return HAL_ERROR;
    }

    if (output[0] == '\0') {
        tool_copy_text(output, output_size, "Command completed with no output.");
    }

    return HAL_OK;
}
#endif

static hal_status_t tool_run_internal_status(const loki_tool_context_t *context, char *output, size_t output_size)
{
    snprintf(output,
             output_size,
             "Loki: %s\nSystem label: %s\nStage: %s\nHeartbeat: %u\nNetwork phase: %u ms\n",
             context->config->dragon_name,
             context->config->device_name,
             dragon_ai_life_stage_name(context->dragon->growth_stage),
             context->heartbeat_ticks,
             context->network_phase_ms);
    return HAL_OK;
}

static void tool_format_gps_value(char *dest, size_t dest_size, const char *label, float value, const char *unit)
{
    snprintf(dest,
             dest_size,
             "%s: %.5f%s%s",
             label,
             value,
             (unit != NULL && unit[0] != '\0') ? " " : "",
             (unit != NULL) ? unit : "");
}

static hal_status_t tool_run_internal_gps(const loki_tool_context_t *context, char *output, size_t output_size)
{
    char lat_line[64];
    char long_line[64];
    char alt_line[64];

    if (context->gps == NULL || !context->gps->has_fix) {
        snprintf(output,
                 output_size,
                 "GPS console\nFix: no\nSource: %s\nLat: --\nLong: --\nAlt: --\n",
                 (context->gps != NULL && context->gps->source[0] != '\0') ? context->gps->source : "awaiting module");
        return HAL_OK;
    }

    tool_format_gps_value(lat_line, sizeof(lat_line), "Lat", context->gps->latitude, "");
    tool_format_gps_value(long_line, sizeof(long_line), "Long", context->gps->longitude, "");
    tool_format_gps_value(alt_line, sizeof(alt_line), "Alt", context->gps->altitude_m, "m");

    snprintf(output,
             output_size,
             "GPS console\nFix: yes\nSource: %s\n%s\n%s\n%s\n",
             context->gps->source[0] != '\0' ? context->gps->source : "runtime",
             lat_line,
             long_line,
             alt_line);
    return HAL_OK;
}

static hal_status_t tool_run_internal_loot(const loki_tool_context_t *context, char *output, size_t output_size)
{
    snprintf(output,
             output_size,
             "Loot journal\nGrowth trophies: %u\nBond: %.2f\nCuriosity: %.2f\nXP: %.2f\n",
             context->dragon->growth_stage,
             context->dragon->bond,
             context->dragon->curiosity,
             context->dragon->xp);
    return HAL_OK;
}

static hal_status_t tool_run_internal_wardrive(const loki_tool_context_t *context, char *output, size_t output_size)
{
    uint32_t scan_percent = 0U;
    char gps_summary[160];
    const char *nearby_networks = "none detected";

    if (context->config->scan_interval_ms > 0U) {
        scan_percent = (context->network_phase_ms * 100U) / context->config->scan_interval_ms;
        if (scan_percent > 100U) {
            scan_percent = 100U;
        }
    }

    if (context->gps != NULL && context->gps->has_fix) {
        snprintf(gps_summary,
                 sizeof(gps_summary),
                 "GPS: %.5f, %.5f alt %.1f m (%s)",
                 context->gps->latitude,
                 context->gps->longitude,
                 context->gps->altitude_m,
                 context->gps->source[0] != '\0' ? context->gps->source : "runtime");
    } else {
        snprintf(gps_summary,
                 sizeof(gps_summary),
                 "GPS: no fix (%s)",
                 (context->gps != NULL && context->gps->source[0] != '\0') ? context->gps->source : "awaiting module");
    }

    if (context->nearby_networks != NULL && context->nearby_networks[0] != '\0') {
        nearby_networks = context->nearby_networks;
    }

    snprintf(output,
             output_size,
             "Wardrive journal\n"
             "Mode: passive scan staging\n"
             "Preferred SSID: %s\n"
             "Scan cadence: %u ms\n"
             "Current scan phase: %u%%\n"
             "Heartbeat: %u\n"
             "Nearby networks: %u\n"
             "SSIDs: %s\n"
             "%s\n"
             "Note: this tool is a safe passive logging placeholder for future GPS tagging.\n",
             context->config->preferred_ssid[0] != '\0' ? context->config->preferred_ssid : "(none)",
             context->config->scan_interval_ms,
             scan_percent,
             context->heartbeat_ticks,
             context->nearby_network_count,
             nearby_networks,
             gps_summary);
    return HAL_OK;
}

static hal_status_t tool_run_internal_peripherals(const loki_tool_context_t *context,
                                                  char *output,
                                                  size_t output_size)
{
    const char *gps_source = "awaiting module";
    const char *nearby_networks = "none detected";

    if (context->gps != NULL && context->gps->source[0] != '\0') {
        gps_source = context->gps->source;
    }
    if (context->nearby_networks != NULL && context->nearby_networks[0] != '\0') {
        nearby_networks = context->nearby_networks;
    }

    snprintf(output,
             output_size,
             "Peripheral console\n"
             "Display backend: %s\n"
             "Framebuffer: %s\n"
             "GPS fix: %s\n"
             "GPS source: %s\n"
             "Nearby Wi-Fi count: %u\n"
             "Nearby Wi-Fi: %s\n"
             "Hardware tests: TFT=%s Flash=%s EEPROM=%s Flipper=%s\n"
             "Safe probes: lsusb, i2cdetect -y <bus>, gpioinfo, ls /dev/tty\n",
             context->config->display.backend,
             context->config->display.framebuffer_device,
             (context->gps != NULL && context->gps->has_fix) ? "yes" : "no",
             gps_source,
             context->nearby_network_count,
             nearby_networks,
             context->config->hardware.test_tft ? "on" : "off",
             context->config->hardware.test_flash ? "on" : "off",
             context->config->hardware.test_eeprom ? "on" : "off",
             context->config->hardware.test_flipper ? "on" : "off");
    return HAL_OK;
}

static hal_status_t tool_run_internal_usb(const loki_tool_context_t *context,
                                          char *output,
                                          size_t output_size)
{
    (void)context;

#if PLATFORM == PLATFORM_LINUX
    return tool_capture_fixed_command("lsusb 2>&1", "USB console", output, output_size);
#else
    snprintf(output, output_size, "USB console\nThis probe is only available on Linux SBC targets.\n");
    return HAL_NOT_SUPPORTED;
#endif
}

static hal_status_t tool_run_internal_i2c(const loki_tool_context_t *context,
                                          char *output,
                                          size_t output_size)
{
    (void)context;

#if PLATFORM == PLATFORM_LINUX
    return tool_capture_fixed_command("i2cdetect -l 2>&1", "I2C console", output, output_size);
#else
    snprintf(output, output_size, "I2C console\nThis probe is only available on Linux SBC targets.\n");
    return HAL_NOT_SUPPORTED;
#endif
}

static hal_status_t tool_run_internal_uart(const loki_tool_context_t *context,
                                           char *output,
                                           size_t output_size)
{
    (void)context;

#if PLATFORM == PLATFORM_LINUX
    return tool_capture_fixed_command("sh -lc \"ls /dev/ttyAMA* /dev/ttyS* /dev/ttyUSB* /dev/ttyACM* 2>/dev/null\"",
                                      "UART console",
                                      output,
                                      output_size);
#else
    snprintf(output, output_size, "UART console\nThis probe is only available on Linux SBC targets.\n");
    return HAL_NOT_SUPPORTED;
#endif
}

static hal_status_t tool_run_internal_gpio(const loki_tool_context_t *context,
                                           char *output,
                                           size_t output_size)
{
    (void)context;

#if PLATFORM == PLATFORM_LINUX
    return tool_capture_fixed_command("gpioinfo 2>&1", "GPIO console", output, output_size);
#else
    snprintf(output, output_size, "GPIO console\nThis probe is only available on Linux SBC targets.\n");
    return HAL_NOT_SUPPORTED;
#endif
}

#if PLATFORM == PLATFORM_LINUX
static hal_status_t tool_run_internal_nmap(const loki_tool_context_t *context,
                                           const char *query,
                                           char *output,
                                           size_t output_size)
{
    char target[64];
    char mode[16];
    char ports[32];
    char command[256];

    if (!tool_query_get_value(query, "target", target, sizeof(target)) || !tool_is_scan_target_valid(target)) {
        snprintf(output, output_size, "Nmap wrapper requires a valid target query parameter.");
        return HAL_INVALID_PARAM;
    }

    if (!tool_query_get_value(query, "mode", mode, sizeof(mode))) {
        tool_copy_text(mode, sizeof(mode), "ping");
    }
    if (!tool_is_scan_mode_valid(mode)) {
        snprintf(output, output_size, "Nmap wrapper mode must be ping, quick, or service.");
        return HAL_INVALID_PARAM;
    }

    if (strcmp(mode, "service") == 0) {
        if (!tool_query_get_value(query, "ports", ports, sizeof(ports)) || !tool_is_port_list_valid(ports)) {
            snprintf(output, output_size, "Service mode requires a valid ports list.");
            return HAL_INVALID_PARAM;
        }
        snprintf(command, sizeof(command), "nmap -Pn -sV -p %s -- %s 2>&1", ports, target);
    } else if (strcmp(mode, "quick") == 0) {
        snprintf(command, sizeof(command), "nmap -Pn --top-ports 20 -- %s 2>&1", target);
    } else {
        snprintf(command, sizeof(command), "nmap -sn -- %s 2>&1", target);
    }

    return tool_run_external_command(command, output, output_size);
}

static hal_status_t tool_run_internal_custom_scan(const loki_tool_context_t *context,
                                                  const char *query,
                                                  char *output,
                                                  size_t output_size)
{
    char target[64];
    char profile[16];
    char ports[32];
    char command[256];

    if (!tool_query_get_value(query, "target", target, sizeof(target)) || !tool_is_scan_target_valid(target)) {
        snprintf(output, output_size, "Custom scan requires a valid target query parameter.");
        return HAL_INVALID_PARAM;
    }

    if (!tool_query_get_value(query, "profile", profile, sizeof(profile))) {
        tool_copy_text(profile, sizeof(profile), "quick");
    }

    if (strcmp(profile, "quick") == 0) {
        snprintf(command, sizeof(command), "nmap -Pn -T4 --top-ports 20 -- %s 2>&1", target);
    } else if (strcmp(profile, "service") == 0) {
        if (!tool_query_get_value(query, "ports", ports, sizeof(ports)) || !tool_is_port_list_valid(ports)) {
            snprintf(output, output_size, "Service profile requires a valid ports list.");
            return HAL_INVALID_PARAM;
        }
        snprintf(command, sizeof(command), "nmap -Pn -sV -p %s -- %s 2>&1", ports, target);
    } else if (strcmp(profile, "udp-top") == 0) {
        snprintf(command, sizeof(command), "nmap -Pn -sU --top-ports 10 -- %s 2>&1", target);
    } else {
        snprintf(output, output_size, "Custom scan profile must be quick, service, or udp-top.");
        return HAL_INVALID_PARAM;
    }

    return tool_run_external_command(command, output, output_size);
}
#endif

hal_status_t loki_tool_run(const loki_tool_config_t *tool,
                           const loki_tool_context_t *context,
                           char *output,
                           size_t output_size)
{
    const char *command;
    const char *query;

    if (tool == NULL || context == NULL || output == NULL || output_size == 0U) {
        return HAL_INVALID_PARAM;
    }

    if (!tool->enabled) {
        snprintf(output, output_size, "Tool is disabled.");
        return HAL_NOT_READY;
    }

    command = tool->command;
    query = strchr(command, '?');
    if (strncmp(command, "internal://status", 17) == 0) {
        return tool_run_internal_status(context, output, output_size);
    }
    if (strncmp(command, "internal://loot", 15) == 0) {
        return tool_run_internal_loot(context, output, output_size);
    }
    if (strncmp(command, "internal://wardrive", 19) == 0) {
        return tool_run_internal_wardrive(context, output, output_size);
    }
    if (strncmp(command, "internal://gps", 14) == 0) {
        return tool_run_internal_gps(context, output, output_size);
    }
    if (strncmp(command, "internal://peripherals", 23) == 0) {
        return tool_run_internal_peripherals(context, output, output_size);
    }
    if (strncmp(command, "internal://usb", 15) == 0) {
        return tool_run_internal_usb(context, output, output_size);
    }
    if (strncmp(command, "internal://i2c", 15) == 0) {
        return tool_run_internal_i2c(context, output, output_size);
    }
    if (strncmp(command, "internal://uart", 16) == 0) {
        return tool_run_internal_uart(context, output, output_size);
    }
    if (strncmp(command, "internal://gpio", 16) == 0) {
        return tool_run_internal_gpio(context, output, output_size);
    }
    if (strncmp(command, "internal://nmap", 15) == 0) {
#if PLATFORM == PLATFORM_LINUX
        return tool_run_internal_nmap(context, (query != NULL) ? query + 1 : "", output, output_size);
#else
        snprintf(output, output_size, "Nmap wrapper is only available on Linux SBC targets.");
        return HAL_NOT_SUPPORTED;
#endif
    }
    if (strncmp(command, "internal://custom_scan", 22) == 0) {
#if PLATFORM == PLATFORM_LINUX
        return tool_run_internal_custom_scan(context, (query != NULL) ? query + 1 : "", output, output_size);
#else
        snprintf(output, output_size, "Custom scan wrapper is only available on Linux SBC targets.");
        return HAL_NOT_SUPPORTED;
#endif
    }

#if PLATFORM == PLATFORM_LINUX
    if (strncmp(command, "safe://", 7) == 0) {
        return tool_run_external_command(command + 7, output, output_size);
    }
    if (strncmp(command, "advanced://", 11) == 0) {
        return tool_run_external_command(command + 11, output, output_size);
    }
#endif

#if PLATFORM == PLATFORM_LINUX
    return tool_run_external_command(command, output, output_size);
#else
    snprintf(output, output_size, "External commands are only supported on Linux targets.");
    return HAL_NOT_SUPPORTED;
#endif
}
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

static bool tool_command_has_safe_characters(const char *command)
{
    size_t index;

    for (index = 0; command[index] != '\0'; index++) {
        unsigned char character = (unsigned char)command[index];
        if (isalnum(character) || character == ' ' || character == '/' || character == '-' ||
            character == '_' || character == '.' || character == ':' ) {
            continue;
        }
        return false;
    }

    return true;
}

static bool tool_command_is_approved(const char *command)
{
    static const char *approved_prefixes[] = {
        "uptime",
        "uname ",
        "uname",
        "hostname",
        "ip ",
        "iwgetid",
        "vcgencmd",
    };
    size_t index;

    if (command == NULL || command[0] == '\0') {
        return false;
    }

    if (!tool_command_has_safe_characters(command)) {
        return false;
    }

    for (index = 0; index < sizeof(approved_prefixes) / sizeof(approved_prefixes[0]); index++) {
        size_t prefix_length = strlen(approved_prefixes[index]);
        if (strncmp(command, approved_prefixes[index], prefix_length) == 0) {
            return true;
        }
    }

    return false;
}

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

    if (context->config->scan_interval_ms > 0U) {
        scan_percent = (context->network_phase_ms * 100U) / context->config->scan_interval_ms;
        if (scan_percent > 100U) {
            scan_percent = 100U;
        }
    }

    snprintf(output,
             output_size,
             "Wardrive journal\n"
             "Mode: passive scan staging\n"
             "Preferred SSID: %s\n"
             "Scan cadence: %u ms\n"
             "Current scan phase: %u%%\n"
             "Heartbeat: %u\n"
             "GPS: not installed yet\n"
             "Note: this tool is a safe passive logging placeholder for future GPS tagging.\n",
             context->config->preferred_ssid[0] != '\0' ? context->config->preferred_ssid : "(none)",
             context->config->scan_interval_ms,
             scan_percent,
             context->heartbeat_ticks);
    return HAL_OK;
}

#if PLATFORM == PLATFORM_LINUX
static hal_status_t tool_run_safe_command(const char *command, char *output, size_t output_size)
{
    FILE *pipe;
    size_t used = 0U;

    if (!tool_command_is_approved(command)) {
        snprintf(output, output_size, "Command rejected. Use an approved safe command.");
        return HAL_NOT_SUPPORTED;
    }

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

    pclose(pipe);

    if (output[0] == '\0') {
        tool_copy_text(output, output_size, "Command completed with no output.");
    }

    return HAL_OK;
}
#endif

hal_status_t loki_tool_run(const loki_tool_config_t *tool,
                           const loki_tool_context_t *context,
                           char *output,
                           size_t output_size)
{
    const char *command;

    if (tool == NULL || context == NULL || output == NULL || output_size == 0U) {
        return HAL_INVALID_PARAM;
    }

    if (!tool->enabled) {
        snprintf(output, output_size, "Tool is disabled.");
        return HAL_NOT_READY;
    }

    command = tool->command;
    if (strncmp(command, "internal://status", 17) == 0) {
        return tool_run_internal_status(context, output, output_size);
    }
    if (strncmp(command, "internal://loot", 15) == 0) {
        return tool_run_internal_loot(context, output, output_size);
    }
    if (strncmp(command, "internal://wardrive", 19) == 0) {
        return tool_run_internal_wardrive(context, output, output_size);
    }

#if PLATFORM == PLATFORM_LINUX
    if (strncmp(command, "safe://", 7) == 0) {
        return tool_run_safe_command(command + 7, output, output_size);
    }
#endif

    snprintf(output, output_size, "Tool command is not supported by the safe runner.");
    return HAL_NOT_SUPPORTED;
}
/**
 * @file web_ui.c
 * @brief Lightweight embedded web UI for Loki.
 */

#include "web_ui.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "platform.h"

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

typedef pthread_t web_ui_thread_t;
typedef pthread_mutex_t web_ui_mutex_t;

static void web_ui_send_status_response(int client_fd,
                                        int status_code,
                                        const char *status_text,
                                        const char *content_type,
                                        const char *body);
static void web_ui_send_response(int client_fd, const char *content_type, const char *body);
static void web_ui_json_append_escaped(char *dest, size_t dest_size, const char *src);
static void web_ui_append_format(char *dest, size_t dest_size, const char *format, ...);
static bool web_ui_request_is_localhost(int client_fd, char *remote_address, size_t remote_address_size);

static void web_ui_free_snapshot_tools(web_ui_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    free(snapshot->tools);
    snapshot->tools = NULL;
    snapshot->tool_count = 0U;
}

static hal_status_t web_ui_clone_snapshot(web_ui_snapshot_t *dest, const web_ui_snapshot_t *src)
{
    size_t bytes;

    if (dest == NULL || src == NULL) {
        return HAL_INVALID_PARAM;
    }

    memset(dest, 0, sizeof(*dest));
    *dest = *src;
    dest->tools = NULL;
    if (src->tool_count == 0U || src->tools == NULL) {
        dest->tool_count = 0U;
        return HAL_OK;
    }

    bytes = src->tool_count * sizeof(loki_tool_config_t);
    dest->tools = (loki_tool_config_t *)malloc(bytes);
    if (dest->tools == NULL) {
        dest->tool_count = 0U;
        return HAL_ERROR;
    }

    memcpy(dest->tools, src->tools, bytes);
    return HAL_OK;
}

static void web_ui_copy_string(char *dest, size_t dest_size, const char *src)
{
    if (dest == NULL || dest_size == 0U) {
        return;
    }

    if (src == NULL) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, dest_size - 1U);
    dest[dest_size - 1U] = '\0';
}

static const char *web_ui_mood_name(dragon_mood_t mood)
{
    return dragon_ai_mood_name(mood);
}

static const char *web_ui_action_name(dragon_action_t action)
{
    return dragon_ai_action_name(action);
}

static const char *web_ui_life_stage_name(uint32_t growth_stage)
{
    return dragon_ai_life_stage_name(growth_stage);
}

static void web_ui_append_text(char *dest, size_t dest_size, const char *src)
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

static void web_ui_append_format(char *dest, size_t dest_size, const char *format, ...)
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

static void web_ui_json_append_escaped(char *dest, size_t dest_size, const char *src)
{
    size_t index;

    if (src == NULL) {
        return;
    }

    for (index = 0; src[index] != '\0'; index++) {
        unsigned char character = (unsigned char)src[index];

        switch (character) {
            case '\\':
                web_ui_append_text(dest, dest_size, "\\\\");
                break;
            case '"':
                web_ui_append_text(dest, dest_size, "\\\"");
                break;
            case '\n':
                web_ui_append_text(dest, dest_size, "\\n");
                break;
            case '\r':
                web_ui_append_text(dest, dest_size, "\\r");
                break;
            case '\t':
                web_ui_append_text(dest, dest_size, "\\t");
                break;
            default:
                if (character < 32U) {
                    char escape[7];
                    snprintf(escape, sizeof(escape), "\\u%04x", character);
                    web_ui_append_text(dest, dest_size, escape);
                } else {
                    char plain[2];
                    plain[0] = (char)character;
                    plain[1] = '\0';
                    web_ui_append_text(dest, dest_size, plain);
                }
                break;
        }
    }
}

static void web_ui_build_tool_cards(const web_ui_snapshot_t *snapshot, char *buffer, size_t buffer_size)
{
    size_t index;

    buffer[0] = '\0';
    for (index = 0; index < snapshot->tool_count; index++) {
        char card[1024];
        snprintf(card,
                 sizeof(card),
                    "<div class=\"card\"><h2>Tool %u</h2><label class=\"muted\"><input type=\"checkbox\" name=\"tool_%u_enabled\" %s> Enabled</label><div class=\"grid\"><div><div class=\"muted\">Name</div><input name=\"tool_%u_name\" value=\"%s\" style=\"width:100%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Command</div><input name=\"tool_%u_command\" value=\"%s\" style=\"width:100%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div></div><div class=\"muted\" style=\"margin-top:12px\">Description</div><textarea name=\"tool_%u_description\" style=\"width:100%;min-height:72px;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\">%s</textarea></div>",
                 snapshot->tools[index].enabled ? "chip-ready" : "chip-disabled",
                 snapshot->tools[index].enabled ? "Ready" : "Disabled",
                 (unsigned int)(index + 1U),
                 snapshot->tools[index].name,
                 snapshot->tools[index].description,
                 snapshot->tools[index].command[0] != '\0' ? snapshot->tools[index].command : "Not configured",
                 (unsigned int)(index + 1U));
        web_ui_append_text(buffer, buffer_size, card);
    }
}

static void web_ui_build_tool_forms(const web_ui_snapshot_t *snapshot, char *buffer, size_t buffer_size)
{
    size_t index;

    buffer[0] = '\0';
    for (index = 0; index < snapshot->tool_count; index++) {
        char card[1536];
        snprintf(card,
                 sizeof(card),
                    "<div class=\"card\"><h2>Tool %u</h2><label class=\"muted\"><input type=\"checkbox\" name=\"tool_%u_enabled\" %s> Enabled</label><div class=\"grid\"><div><div class=\"muted\">Name</div><input name=\"tool_%u_name\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Command</div><input name=\"tool_%u_command\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div></div><div class=\"muted\" style=\"margin-top:12px\">Description</div><textarea name=\"tool_%u_description\" style=\"width:100%%;min-height:72px;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\">%s</textarea></div>",
                 (unsigned int)(index + 1U),
                 (unsigned int)(index + 1U),
                 snapshot->tools[index].enabled ? "checked" : "",
                 (unsigned int)(index + 1U),
                 snapshot->tools[index].name,
                 (unsigned int)(index + 1U),
                    snapshot->tools[index].command[0] != '\0' ? snapshot->tools[index].command : "Not configured",
                 (unsigned int)(index + 1U),
                 snapshot->tools[index].description);
        web_ui_append_text(buffer, buffer_size, card);
    }
}

static void web_ui_send_message_page(int client_fd, const char *title, const char *message)
{
    char body[8192];

    snprintf(body,
             sizeof(body),
             "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>%s</title><style>body{font-family:Verdana,sans-serif;background:#08131c;color:#edf7f5;padding:28px}.card{max-width:760px;margin:0 auto;background:#10202c;border:1px solid rgba(255,255,255,.08);border-radius:18px;padding:24px}a{color:#8ce9c8;text-decoration:none}.pre{white-space:pre-wrap;background:#091722;padding:14px;border-radius:14px}</style></head><body><div class=\"card\"><h1>%s</h1><div class=\"pre\">%s</div><p><a href=\"/\">Return to Loki</a></p></div></body></html>",
             title,
             title,
             message);
    web_ui_send_response(client_fd, "text/html; charset=utf-8", body);
}

static void web_ui_send_status_response(int client_fd,
                                        int status_code,
                                        const char *status_text,
                                        const char *content_type,
                                        const char *body)
{
    char header[512];
    int header_len;
    size_t body_len;

    if (status_text == NULL) {
        status_text = "OK";
    }
    if (content_type == NULL) {
        content_type = "text/plain; charset=utf-8";
    }
    if (body == NULL) {
        body = "";
    }

    body_len = strlen(body);
    header_len = snprintf(header,
                          sizeof(header),
                          "HTTP/1.1 %d %s\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %u\r\n"
                          "Connection: close\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Access-Control-Allow-Methods: GET, POST, PUT, PATCH, OPTIONS\r\n"
                          "Access-Control-Allow-Headers: Content-Type\r\n\r\n",
                          status_code,
                          status_text,
                          content_type,
                          (unsigned int)body_len);

    if (header_len > 0) {
        send(client_fd, header, (size_t)header_len, 0);
        send(client_fd, body, body_len, 0);
    }
}

static void web_ui_send_response(int client_fd, const char *content_type, const char *body)
{
    web_ui_send_status_response(client_fd, 200, "OK", content_type, body);
}

static void web_ui_send_not_found(int client_fd)
{
    web_ui_send_status_response(client_fd, 404, "Not Found", "text/plain; charset=utf-8", "Not Found");
}

static void web_ui_build_json(const web_ui_snapshot_t *snapshot, char *buffer, size_t buffer_size)
{
    buffer[0] = '\0';
    web_ui_append_text(buffer, buffer_size, "{");
    web_ui_append_text(buffer, buffer_size, "\"device_name\":\"");
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->device_name);
    web_ui_append_text(buffer, buffer_size, "\",");
    web_ui_append_text(buffer, buffer_size, "\"dragon_name\":\"");
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->dragon_name);
    web_ui_append_text(buffer, buffer_size, "\",");
    web_ui_append_text(buffer, buffer_size, "\"hostname\":\"");
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->hostname);
    web_ui_append_text(buffer, buffer_size, "\",");
    web_ui_append_text(buffer, buffer_size, "\"primary_ip\":\"");
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->primary_ip);
    web_ui_append_text(buffer, buffer_size, "\",");
    web_ui_append_format(buffer, buffer_size, "\"credits_balance\":%.2f,", snapshot->credits_balance);
    web_ui_append_format(buffer, buffer_size, "\"running_tool_count\":%u,", (unsigned int)snapshot->running_tool_count);
    web_ui_append_format(buffer,
                         buffer_size,
                         "\"gps\":{\"has_fix\":%s,\"latitude\":%.5f,\"longitude\":%.5f,\"altitude_m\":%.2f,\"source\":\"",
                         snapshot->gps_has_fix ? "true" : "false",
                         snapshot->gps_latitude,
                         snapshot->gps_longitude,
                         snapshot->gps_altitude_m);
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->gps_source);
    web_ui_append_text(buffer, buffer_size, "\"},");
    web_ui_append_format(buffer,
                         buffer_size,
                         "\"nearby_network_count\":%u,\"known_network_total\":%u,\"nearby_networks\":\"",
                         snapshot->nearby_network_count,
                         snapshot->known_network_total);
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->nearby_networks);
    web_ui_append_text(buffer, buffer_size, "\",");
    web_ui_append_text(buffer, buffer_size, "\"discovery_message\":\"");
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->discovery_message);
    web_ui_append_text(buffer, buffer_size, "\",");
    web_ui_append_text(buffer, buffer_size, "\"bind_address\":\"");
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->bind_address);
    web_ui_append_format(buffer,
                         buffer_size,
                         "\",\"port\":%u,\"dhcp_enabled\":%s,\"standalone_mode\":%s,\"announce_new_networks\":%s,\"display_ready\":%s,\"preferred_ssid\":\"",
                         snapshot->port,
                         snapshot->dhcp_enabled ? "true" : "false",
                         snapshot->standalone_mode ? "true" : "false",
                         snapshot->announce_new_networks ? "true" : "false",
                         snapshot->display_ready ? "true" : "false");
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->preferred_ssid);
    web_ui_append_text(buffer, buffer_size, "\",");
    web_ui_append_text(buffer, buffer_size, "\"display_backend\":\"");
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->display_backend);
    web_ui_append_text(buffer, buffer_size, "\",");
    web_ui_append_text(buffer, buffer_size, "\"framebuffer_device\":\"");
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->framebuffer_device);
    web_ui_append_format(buffer,
                         buffer_size,
                         "\",\"tool_count\":%u,\"heartbeat_ticks\":%u,\"network_phase_ms\":%u,\"scan_interval_ms\":%u,\"hop_interval_ms\":%u,",
                         (unsigned int)snapshot->tool_count,
                         snapshot->heartbeat_ticks,
                         snapshot->network_phase_ms,
                         snapshot->scan_interval_ms,
                         snapshot->hop_interval_ms);
    web_ui_append_format(buffer,
                         buffer_size,
                         "\"ai\":{\"learning_rate\":%.3f,\"actor_learning_rate\":%.3f,\"critic_learning_rate\":%.3f,\"discount_factor\":%.3f,\"reward_decay\":%.3f,\"entropy_beta\":%.3f,\"policy_temperature\":%.3f,\"tick_interval_ms\":%u},",
                         snapshot->ai_learning_rate,
                         snapshot->ai_actor_learning_rate,
                         snapshot->ai_critic_learning_rate,
                         snapshot->ai_discount_factor,
                         snapshot->ai_reward_decay,
                         snapshot->ai_entropy_beta,
                         snapshot->ai_policy_temperature,
                         snapshot->ai_tick_interval_ms);
    web_ui_append_format(buffer,
                         buffer_size,
                         "\"dragon\":{\"name\":\"",
                         0);
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->dragon.name);
    web_ui_append_format(buffer,
                         buffer_size,
                         "\",\"age_ticks\":%u,\"growth_stage\":%u,\"life_stage\":\"",
                         snapshot->dragon.age_ticks,
                         snapshot->dragon.growth_stage);
    web_ui_json_append_escaped(buffer, buffer_size, web_ui_life_stage_name(snapshot->dragon.growth_stage));
    web_ui_append_format(buffer,
                         buffer_size,
                         "\",\"hunger\":%.3f,\"energy\":%.3f,\"curiosity\":%.3f,\"bond\":%.3f,\"xp\":%.3f,\"last_reward\":%.3f,\"last_advantage\":%.3f,\"policy_confidence\":%.3f,\"policy_entropy\":%.3f,\"value_estimate\":%.3f,\"reward_total\":%.3f,\"reward_average\":%.3f,\"reward_events\":%u,\"speech\":\"",
                         snapshot->dragon.hunger,
                         snapshot->dragon.energy,
                         snapshot->dragon.curiosity,
                         snapshot->dragon.bond,
                         snapshot->dragon.xp,
                         snapshot->dragon.last_reward,
                         snapshot->dragon.last_advantage,
                         snapshot->dragon.policy_confidence,
                         snapshot->dragon.policy_entropy,
                         snapshot->dragon.value_estimate,
                         snapshot->dragon.reward_total,
                         snapshot->dragon.reward_average,
                         snapshot->dragon.reward_events);
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->dragon.speech);
    web_ui_append_text(buffer, buffer_size, "\",\"mood\":\"");
    web_ui_json_append_escaped(buffer, buffer_size, web_ui_mood_name(snapshot->dragon.mood));
    web_ui_append_text(buffer, buffer_size, "\",\"action\":\"");
    web_ui_json_append_escaped(buffer, buffer_size, web_ui_action_name(snapshot->dragon.action));
    web_ui_append_text(buffer, buffer_size, "\"},\"settings\":{\"profile\":\"");
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->dragon_profile);
    web_ui_append_text(buffer, buffer_size, "\",\"temperament\":\"");
    web_ui_json_append_escaped(buffer, buffer_size, snapshot->dragon_temperament);
    web_ui_append_format(buffer,
                         buffer_size,
                         "\",\"plugin_display\":%s,\"plugin_dragon_ai\":%s,\"plugin_network_state\":%s,\"standalone_mode\":%s,\"announce_new_networks\":%s,\"base_energy\":%.3f,\"base_curiosity\":%.3f,\"base_bond\":%.3f,\"base_hunger\":%.3f,\"hunger_rate\":%.3f,\"energy_decay\":%.3f,\"curiosity_rate\":%.3f,\"explore_xp_gain\":%.3f,\"play_xp_gain\":%.3f,\"growth_interval_ms\":%u,\"growth_xp_step\":%.3f,\"egg_stage_max\":%u,\"hatchling_stage_max\":%u,\"discovery_hold_ms\":%u}}",
                         snapshot->plugin_display ? "true" : "false",
                         snapshot->plugin_dragon_ai ? "true" : "false",
                         snapshot->plugin_network_state ? "true" : "false",
                         snapshot->standalone_mode ? "true" : "false",
                         snapshot->announce_new_networks ? "true" : "false",
                         snapshot->dragon_base_energy,
                         snapshot->dragon_base_curiosity,
                         snapshot->dragon_base_bond,
                         snapshot->dragon_base_hunger,
                         snapshot->dragon_hunger_rate,
                         snapshot->dragon_energy_decay,
                         snapshot->dragon_curiosity_rate,
                         snapshot->dragon_explore_xp_gain,
                         snapshot->dragon_play_xp_gain,
                         snapshot->dragon_growth_interval_ms,
                         snapshot->dragon_growth_xp_step,
                         snapshot->dragon_egg_stage_max,
                         snapshot->dragon_hatchling_stage_max,
                         snapshot->discovery_hold_ms);
}

static char *web_ui_build_html(const web_ui_snapshot_t *snapshot)
{
    const char *display_state = snapshot->display_ready ? "ready" : "offline";
    const char *network_state = snapshot->dhcp_enabled ? "dhcp" : "static";
    const char *mood = web_ui_mood_name(snapshot->dragon.mood);
    const char *action = web_ui_action_name(snapshot->dragon.action);
    const char *life_stage = web_ui_life_stage_name(snapshot->dragon.growth_stage);
    const char *gps_fix = snapshot->gps_has_fix ? "locked" : "no fix";
    unsigned int energy_pct = (unsigned int)(snapshot->dragon.energy * 100.0f);
    unsigned int curiosity_pct = (unsigned int)(snapshot->dragon.curiosity * 100.0f);
    unsigned int bond_pct = (unsigned int)(snapshot->dragon.bond * 100.0f);
    unsigned int hunger_pct = (unsigned int)((1.0f - snapshot->dragon.hunger) * 100.0f);
    unsigned int hatch_progress = (snapshot->dragon.growth_stage >= (DRAGON_EGG_STAGE_MAX + 1U)) ?
        100U :
        (snapshot->dragon.growth_stage * 100U) / (DRAGON_EGG_STAGE_MAX + 1U);
    unsigned int loot_trophies = snapshot->dragon.growth_stage;
    unsigned int loot_scans = (snapshot->scan_interval_ms > 0U) ? ((snapshot->heartbeat_ticks * 250U) / snapshot->scan_interval_ms) : 0U;
    const char *stage_art = "🥚";
    char *tool_cards;
    char *tool_forms;
    char *buffer;
    char gps_lat[32];
    char gps_long[32];
    char gps_alt[32];
    char credits_display[32];
    char snippet[12288];
    size_t tool_cards_size;
    size_t tool_forms_size;
    size_t buffer_size;

    if (dragon_ai_life_stage(snapshot->dragon.growth_stage) == DRAGON_LIFE_STAGE_HATCHLING) {
        stage_art = "🐉";
    } else if (dragon_ai_life_stage(snapshot->dragon.growth_stage) == DRAGON_LIFE_STAGE_ADULT) {
        stage_art = "🐲";
    }

    if (snapshot->gps_has_fix) {
        snprintf(gps_lat, sizeof(gps_lat), "%.5f", snapshot->gps_latitude);
        snprintf(gps_long, sizeof(gps_long), "%.5f", snapshot->gps_longitude);
        snprintf(gps_alt, sizeof(gps_alt), "%.1f m", snapshot->gps_altitude_m);
    } else {
        web_ui_copy_string(gps_lat, sizeof(gps_lat), "--");
        web_ui_copy_string(gps_long, sizeof(gps_long), "--");
        web_ui_copy_string(gps_alt, sizeof(gps_alt), "--");
    }

    snprintf(credits_display, sizeof(credits_display), "$ %.2f", snapshot->credits_balance);

    tool_cards_size = (snapshot->tool_count * 640U) + 256U;
    tool_forms_size = (snapshot->tool_count * 1280U) + 512U;
    buffer_size = 49152U + tool_cards_size + tool_forms_size;

    tool_cards = (char *)malloc(tool_cards_size);
    tool_forms = (char *)malloc(tool_forms_size);
    buffer = (char *)malloc(buffer_size);
    if (tool_cards == NULL || tool_forms == NULL || buffer == NULL) {
        free(tool_cards);
        free(tool_forms);
        free(buffer);
        return NULL;
    }

    web_ui_build_tool_cards(snapshot, tool_cards, tool_cards_size);
    web_ui_build_tool_forms(snapshot, tool_forms, tool_forms_size);
    buffer[0] = '\0';

    web_ui_append_text(buffer, buffer_size,
                       "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Loki Control</title><style>"
                       ":root{color-scheme:dark;--bg:#0a0f14;--panel:#1a232c;--panel-2:#232e3a;--line:rgba(255,255,255,.08);--ink:#f6f6f6;--muted:#b6b6b6;--accent:#e6b450;--accent-2:#ff7f50;--danger:#ff4b4b;--glow:rgba(230,180,80,.18);}"
                       "body{margin:0;font-family:'Segoe UI',Verdana,sans-serif;background:linear-gradient(180deg,#232e3a 0%,#1a232c 100%);color:var(--ink);letter-spacing:.01em;min-height:100vh;}"
                       ".wrap{max-width:1240px;margin:0 auto;padding:24px;}"
                       ".nav{display:flex;align-items:center;justify-content:space-between;gap:16px;padding:16px 20px;background:#232e3a;border:1px solid var(--line);border-radius:24px;position:sticky;top:12px;z-index:6;}"
                       ".brand{display:flex;align-items:center;gap:14px;}.glyph{width:48px;height:48px;border-radius:16px;display:grid;place-items:center;background:linear-gradient(135deg,#e6b450,#ff7f50);font-size:24px;}"
                       ".nav-meta{display:flex;align-items:center;gap:8px;color:var(--muted);font-size:11px;text-transform:uppercase;letter-spacing:.12em;}"
                       ".tabs{display:flex;gap:10px;flex-wrap:wrap;}.tab{padding:10px 14px;border-radius:999px;border:1px solid var(--line);background:transparent;color:var(--muted);cursor:pointer;font-size:13px;text-transform:uppercase;letter-spacing:.08em;}"
                       ".tab.active{background:var(--accent);color:#232e3a;border-color:transparent;font-weight:800;}.page{display:none;padding-top:20px;}.page.active{display:block;}"
                       ".hero{display:grid;grid-template-columns:1.55fr 1fr;gap:18px;}.card{background:var(--panel);border:1px solid var(--line);border-radius:26px;padding:22px;box-shadow:0 22px 60px rgba(0,0,0,.32);}"
                       "h1{margin:0 0 10px;font-size:48px;letter-spacing:.02em;}h2{margin:0;font-size:24px;}.muted{color:var(--muted);font-size:14px;}"
                       ".metric{display:flex;justify-content:space-between;padding:12px 0;border-bottom:1px solid var(--line);gap:12px;}.metric:last-child{border-bottom:none;}"
                       ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:18px;margin-top:18px;}.pill{display:inline-block;padding:7px 12px;border-radius:999px;background:var(--accent);color:#232e3a;text-transform:uppercase;font-size:12px;letter-spacing:.08em;}"
                       ".big-stage{font-size:78px;line-height:1;margin:14px 0;}.bar{height:12px;background:var(--panel-2);border-radius:999px;overflow:hidden;margin-top:8px;}.fill{height:100%;background:linear-gradient(90deg,var(--accent-2),var(--accent));}"
                       ".stat{font-size:28px;font-weight:700;margin-top:8px;}.stack{display:grid;gap:14px;}.subgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin-top:16px;}"
                       ".loot-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:18px;}.loot-card{padding:20px;border-radius:22px;background:var(--panel-2);border:1px solid var(--accent);box-shadow:inset 0 1px 0 var(--line);}.loot-card .stat{font-size:24px;}"
                       ".chip{display:inline-flex;gap:8px;align-items:center;padding:8px 12px;border-radius:999px;background:var(--accent-2);color:#232e3a;font-size:12px;text-transform:uppercase;letter-spacing:.08em;}"
                       ".speech{margin-top:14px;padding:16px 18px;border-radius:18px;background:var(--panel-2);border:1px solid var(--accent);font-size:15px;line-height:1.55;min-height:52px;box-shadow:0 0 0 1px var(--line) inset;}"
                       ".speech-remarks{min-height:122px;margin-top:10px;}"
                       ".status-strip{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin-top:18px;}.status-tile{padding:14px 16px;border-radius:16px;background:var(--panel-2);border:1px solid var(--accent);}"
                       ".mono{font-family:Consolas,monospace;}.actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:14px;}.actions .tab{text-decoration:none;}input,textarea{box-sizing:border-box;font:inherit;}a{color:var(--accent-2);text-decoration:none;}"
                       ".section-head{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:8px;}"
                       ".settings-note{padding:12px 14px;border-radius:16px;background:var(--glow);border:1px solid var(--accent);color:var(--muted);margin-top:14px;}"
                       "@media (max-width:860px){.hero{grid-template-columns:1fr;}.nav{flex-direction:column;align-items:flex-start;}}@media (max-width:560px){.hero-stats,.pulse-grid{grid-template-columns:1fr;}}"
                       "</style>"
                       "<div style='text-align:center;margin-top:12px;color:#e6b450;font-size:15px;'>Theme: The Harvester by <a href='https://github.com/harvester-theme' style='color:#ff7f50;text-decoration:underline;'>Harvester Author</a> &mdash; Loki UI adaptation</div>"
                       "<script>"
                       "function showTab(tab){document.querySelectorAll('.tab').forEach(function(el){el.classList.toggle('active',el.dataset.tab===tab);});document.querySelectorAll('.page').forEach(function(el){el.classList.toggle('active',el.id===tab);});}"
                       "function setText(id,value){var el=document.getElementById(id);if(el){el.textContent=String(value);}}"
                       "function setWidth(id,value){var el=document.getElementById(id);if(el){el.style.width=Math.max(0,Math.min(100,value))+'%';}}"
                       "function setHTML(id,value){var el=document.getElementById(id);if(el){el.innerHTML=value;}}"
                       "function stageGlyph(stage){if(stage==='Adult'){return '<svg width=\"132\" height=\"132\" viewBox=\"0 0 132 132\"><defs><linearGradient id=\"s1\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\"><stop offset=\"0%\" stop-color=\"#7f62ff\"/><stop offset=\"100%\" stop-color=\"#2a1f56\"/></linearGradient><linearGradient id=\"g1\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\"><stop offset=\"0%\" stop-color=\"#ffd27f\"/><stop offset=\"100%\" stop-color=\"#d78c40\"/></linearGradient></defs><ellipse cx=\"66\" cy=\"70\" rx=\"45\" ry=\"38\" fill=\"url(#g1)\" stroke=\"#4228a3\" stroke-width=\"3\"/><path d=\"M50 60 Q66 35 82 60\" fill=\"#bea2ff\" opacity=\"0.9\"/><circle cx=\"50\" cy=\"55\" r=\"7\" fill=\"#fff\"/><circle cx=\"50\" cy=\"55\" r=\"3\" fill=\"#1d1635\"/><circle cx=\"82\" cy=\"52\" r=\"8\" fill=\"#fff\"/><circle cx=\"82\" cy=\"52\" r=\"3\" fill=\"#1d1635\"/><path d=\"M58 82 Q66 92 74 82\" stroke=\"#fee2b6\" stroke-width=\"4\" fill=\"none\" stroke-linecap=\"round\"/></svg>';}if(stage==='Hatchling'){return '<svg width=\"132\" height=\"132\" viewBox=\"0 0 132 132\"><defs><linearGradient id=\"s2\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\"><stop offset=\"0%\" stop-color=\"#ffd68d\"/><stop offset=\"100%\" stop-color=\"#ec9c4b\"/></linearGradient></defs><ellipse cx=\"66\" cy=\"75\" rx=\"35\" ry=\"32\" fill=\"url(#s2)\" stroke=\"#7f5934\" stroke-width=\"3\"/><path d=\"M52 68 Q66 45 80 68\" fill=\"#ffb97a\" opacity=\"0.85\"/><circle cx=\"54\" cy=\"70\" r=\"6\" fill=\"#fff\"/><circle cx=\"54\" cy=\"70\" r=\"2\" fill=\"#1f1712\"/><circle cx=\"78\" cy=\"68\" r=\"5\" fill=\"#fff\"/><circle cx=\"78\" cy=\"68\" r=\"2\" fill=\"#1f1712\"/><path d=\"M56 88 Q66 100 76 88\" stroke=\"#fff2d8\" stroke-width=\"3\" fill=\"none\" stroke-linecap=\"round\"/></svg>';}return '<svg width=\"132\" height=\"132\" viewBox=\"0 0 132 132\"><defs><linearGradient id=\"s3\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\"><stop offset=\"0%\" stop-color=\"#e9d8b0\"/><stop offset=\"100%\" stop-color=\"#b28c5a\"/></linearGradient></defs><ellipse cx=\"66\" cy=\"78\" rx=\"32\" ry=\"38\" fill=\"url(#s3)\" stroke=\"#6a513e\" stroke-width=\"3\"/><path d=\"M50 72 Q66 38 82 72\" fill=\"#f7d6b0\" opacity=\"0.8\"/><circle cx=\"56\" cy=\"74\" r=\"6\" fill=\"#fff\"/><circle cx=\"56\" cy=\"74\" r=\"2\" fill=\"#2e2520\"/><circle cx=\"76\" cy=\"72\" r=\"5\" fill=\"#fff\"/><circle cx=\"76\" cy=\"72\" r=\"2\" fill=\"#2e2520\"/></svg>';}}"
                       "function dragonSpriteMarkup(stage){return '<svg viewBox=\"0 0 220 140\" class=\"dragon-sprite\"><defs><linearGradient id=\"bodyGrad\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\"><stop offset=\"0%\" stop-color=\"#7a50f1\"/><stop offset=\"100%\" stop-color=\"#3d2973\"/></linearGradient></defs><path d=\"M40 90 Q60 50 100 70 Q130 50 190 85\" fill=\"url(#bodyGrad)\" stroke=\"#5a3db0\" stroke-width=\"3\" stroke-linecap=\"round\"/><circle cx=\"80\" cy=\"70\" r=\"6\" fill=\"#fff\"/><circle cx=\"80\" cy=\"70\" r=\"2\" fill=\"#1d1635\"/><circle cx=\"140\" cy=\"65\" r=\"6\" fill=\"#fff\"/><circle cx=\"140\" cy=\"65\" r=\"2\" fill=\"#1d1635\"/><path d=\"M90 95 Q120 110 150 95\" stroke=\"#fee2b6\" stroke-width=\"4\" fill=\"none\" stroke-linecap=\"round\"/></svg>';}"
                       "function roamDragon(){var dragon=document.getElementById('loki-dragon');var bubble=document.getElementById('dragon-chat');if(!dragon||!bubble){return;}var ticker=Date.now()/520;var bob=Math.sin(ticker)*11;var drift=Math.sin(ticker/1.7)*6;var sway=Math.sin(ticker/2.4)*3;var flip=(Math.cos(ticker/2)>0)?1:-1;dragon.style.right='18px';dragon.style.bottom=(18+bob)+'px';dragon.style.left='auto';dragon.style.top='auto';dragon.style.transform='scaleX('+flip+') translateX('+drift+'px) rotate('+sway+'deg)';dragon.style.boxShadow='0 '+(3+Math.abs(bob)/6)+'px '+(12+Math.abs(drift))+'px rgba(102,85,255,0.16)';bubble.style.right='20px';bubble.style.bottom=(142+bob)+'px';bubble.style.left='auto';bubble.style.top='auto';}""
                       "function sassyLine(){var stage=dragonState.lifeStage||'Egg';var mood=String(dragonState.mood||'calm').toLowerCase();var action=String(dragonState.action||'idle').toLowerCase();var name=dragonState.name||'Loki';if(stage==='Adult'){if(mood.indexOf('curious')>=0){return name+': I have the network mapped. No packet sneaks past me.';}if(action.indexOf('explore')>=0){return name+': I ride the ether and score every secret. Feed me new tools.';}if(action.indexOf('scan')>=0){return name+': Hunting open ports with a grin. This domain is mine to defend.';}return name+': I am big, savvy, and ready to earn more XP.';}if(stage==='Hatchling'){if(mood.indexOf('play')>=0||action.indexOf('play')>=0){return name+': Tiny claws, huge dreams. Watch me learn and level up.';}if(action.indexOf('explore')>=0){return name+': Sneaking through packets like a curious cub.';}return name+': Growing faster every time you use my features.';}return name+': Hatching soon. Keep the power on and I will emerge with points to burn.';}"
                       "function updateDragonPersona(data){var dragon=document.getElementById('loki-dragon');var bubble=document.getElementById('dragon-chat');if(!data||!data.dragon){return;}dragonState.lifeStage=data.dragon.life_stage||'Egg';dragonState.mood=data.dragon.mood||'calm';dragonState.action=data.dragon.action||'idle';dragonState.name=data.dragon_name||'Loki';if(dragon){dragon.innerHTML=dragonSpriteMarkup(dragonState.lifeStage);}if(bubble){bubble.innerHTML='<strong>'+dragonState.name+'</strong><br>'+sassyLine();}}"
                       "async function refreshStatus(){try{var response=await fetch('/api/status',{cache:'no-store'});if(!response.ok){return;}var data=await response.json();setText('live-navbar-name',data.dragon_name);setText('live-stage-chip',String(data.dragon.life_stage).toUpperCase());setText('live-title',data.dragon_name);setText('live-mood-action','Mood '+data.dragon.mood+', action '+data.dragon.action+', growth stage '+data.dragon.growth_stage);setHTML('live-stage-art',stageGlyph(data.dragon.life_stage));setText('live-stage-line',data.dragon.life_stage+' Stage '+data.dragon.growth_stage);setText('live-xp',Number(data.dragon.xp).toFixed(1));setText('live-reward',Number(data.dragon.last_reward).toFixed(2));setText('live-display',data.display_ready?'ready':'offline');setText('live-network',data.standalone_mode?'standalone scout':(data.dhcp_enabled?'dhcp':'static'));setText('live-speech',data.dragon.speech);setText('live-remarks',data.dragon.speech);setText('live-attitude',data.dragon.mood);setText('live-heartbeat',data.heartbeat_ticks);setText('live-phase',data.network_phase_ms+' ms');setText('live-confidence',Number(data.dragon.policy_confidence).toFixed(2));setText('live-advantage',Number(data.dragon.last_advantage).toFixed(2));setText('live-value',Number(data.dragon.value_estimate).toFixed(2));setText('live-entropy',Number(data.dragon.policy_entropy).toFixed(3));setText('live-reward-total',Number(data.dragon.reward_total).toFixed(2));setText('live-reward-average',Number(data.dragon.reward_average).toFixed(3));setText('live-reward-events',data.dragon.reward_events);setText('live-tool-count',data.tool_count);setText('live-tool-count-detail',data.tool_count);setText('live-ip',data.primary_ip);setText('live-ip-detail',data.primary_ip);setText('live-gps-fix',data.gps.has_fix?'locked':'no fix');setText('live-gps-source',data.gps.source);setText('live-gps-lat',data.gps.has_fix?Number(data.gps.latitude).toFixed(5):'--');setText('live-gps-long',data.gps.has_fix?Number(data.gps.longitude).toFixed(5):'--');setText('live-gps-alt',data.gps.has_fix?Number(data.gps.altitude_m).toFixed(1)+' m':'--');setText('live-nearby-count',data.nearby_network_count);setText('live-nearby-count-copy',data.nearby_network_count);setText('live-known-count',data.known_network_total);setText('live-known-count-telemetry',data.known_network_total);setText('live-nearby-list',data.nearby_networks);setText('live-discovery',data.discovery_message||'waiting for new networks');setText('live-scan',data.network_phase_ms+' / '+data.scan_interval_ms+' ms');setWidth('live-hatch-fill',data.dragon.growth_stage>=4?100:Math.round((data.dragon.growth_stage*100)/4));setWidth('live-energy-fill',Math.round(data.dragon.energy*100));setWidth('live-curiosity-fill',Math.round(data.dragon.curiosity*100));setWidth('live-bond-fill',Math.round(data.dragon.bond*100));setWidth('live-hunger-fill',Math.round((1-data.dragon.hunger)*100));}catch(error){}}"
                       "async function refreshStatus(){try{var response=await fetch('/api/status',{cache:'no-store'});if(!response.ok){return;}var data=await response.json();var credits=(Number(data.dragon.reward_total)||0).toFixed(2);setText('live-navbar-name',data.dragon_name);setText('live-stage-chip',String(data.dragon.life_stage).toUpperCase());setText('live-title',data.dragon_name);setText('live-mood-action','Mood '+data.dragon.mood+', action '+data.dragon.action+', growth stage '+data.dragon.growth_stage);setHTML('live-stage-art',stageGlyph(data.dragon.life_stage));setText('live-stage-line',data.dragon.life_stage+' Stage '+data.dragon.growth_stage);setText('live-xp',Number(data.dragon.xp).toFixed(1));setText('live-reward',Number(data.dragon.last_reward).toFixed(2));setText('live-display',data.display_ready?'ready':'offline');setText('live-network',data.standalone_mode?'standalone scout':(data.dhcp_enabled?'dhcp':'static'));setText('live-speech',data.dragon.speech);setText('live-remarks',data.dragon.speech+'  '+sassyLine());setText('live-attitude',data.dragon.mood);setText('live-mood-main',data.dragon.mood);setText('live-action-main',data.dragon.action);setText('live-confidence-main',Number(data.dragon.policy_confidence).toFixed(2));setText('live-advantage-main',Number(data.dragon.last_advantage).toFixed(2));setText('live-entropy-main',Number(data.dragon.policy_entropy).toFixed(3));setText('live-credits','¢ '+credits);setText('live-credits-main','¢ '+credits);setText('live-credits-display','¢ '+credits);setText('live-credits-edit-value',credits);setText('live-heartbeat',data.heartbeat_ticks);setText('live-phase',data.network_phase_ms+' ms');setText('live-confidence',Number(data.dragon.policy_confidence).toFixed(2));setText('live-advantage',Number(data.dragon.last_advantage).toFixed(2));setText('live-value',Number(data.dragon.value_estimate).toFixed(2));setText('live-entropy',Number(data.dragon.policy_entropy).toFixed(3));setText('live-reward-total',Number(data.dragon.reward_total).toFixed(2));setText('live-reward-average',Number(data.dragon.reward_average).toFixed(3));setText('live-reward-events',data.dragon.reward_events);setText('live-tool-count',data.tool_count);setText('live-tool-count-detail',data.tool_count);setText('live-ip',data.primary_ip);setText('live-ip-detail',data.primary_ip);setText('live-gps-fix',data.gps.has_fix?'locked':'no fix');setText('live-gps-source',data.gps.source);setText('live-gps-lat',data.gps.has_fix?Number(data.gps.latitude).toFixed(5):'--');setText('live-gps-long',data.gps.has_fix?Number(data.gps.longitude).toFixed(5):'--');setText('live-gps-alt',data.gps.has_fix?Number(data.gps.altitude_m).toFixed(1)+' m':'--');setText('live-nearby-count',data.nearby_network_count);setText('live-nearby-count-copy',data.nearby_network_count);setText('live-known-count',data.known_network_total);setText('live-known-count-telemetry',data.known_network_total);setText('live-nearby-list',data.nearby_networks);setText('live-discovery',data.discovery_message||'waiting for new networks');setText('live-scan',data.network_phase_ms+' / '+data.scan_interval_ms+' ms');setWidth('live-hatch-fill',data.dragon.growth_stage>=4?100:Math.round((data.dragon.growth_stage*100)/4));setWidth('live-energy-fill',Math.round(data.dragon.energy*100));setWidth('live-curiosity-fill',Math.round(data.dragon.curiosity*100));setWidth('live-bond-fill',Math.round(data.dragon.bond*100));setWidth('live-hunger-fill',Math.round((1-data.dragon.hunger)*100));updateDragonPersona(data);}catch(error){}}"
                       "async function resetNetworkMemory(){try{var response=await fetch('/api/networks/reset',{method:'POST'});if(!response.ok){return;}await refreshStatus();}catch(error){}}"
                       "async function updateCredits(newValue){try{var response=await fetch('/api/credits',{method:'PATCH',headers:{'Content-Type':'application/json'},body:JSON.stringify({credits:parseFloat(newValue)})});if(!response.ok){alert('Failed to update credits');return;}var data=await response.json();if(data.ok){setText('live-credits-edit-value',data.credits.toFixed(2));setText('live-credits-display',data.credits.toFixed(2));}else{alert('Error: '+(data.error||'unknown'));}}catch(error){alert('Network error: '+error);}}"
                       "function showCreditsEditor(){var editor=document.getElementById('credits-editor');if(editor){editor.style.display=(editor.style.display==='none'?'block':'none');}}"
                       "function addCredits(amount){var current=parseFloat(document.getElementById('live-credits-edit-value').textContent)||0;updateCredits(Math.max(0,current+parseFloat(amount)));}"
                       "function subtractCredits(amount){var current=parseFloat(document.getElementById('live-credits-edit-value').textContent)||0;updateCredits(Math.max(0,current-parseFloat(amount)));}"
                       "function setCreditsValue(){var input=document.getElementById('credits-input');if(input){updateCredits(input.value);}}"
                       "window.addEventListener('DOMContentLoaded',function(){document.querySelectorAll('.tab').forEach(function(el){el.addEventListener('click',function(){showTab(el.dataset.tab);});});showTab('home');refreshStatus();window.setInterval(refreshStatus,2000);window.setInterval(roamDragon,4200);roamDragon();});"
                       "</script></head><body><div class=\"wrap\">");

    snprintf(snippet,
             sizeof(snippet),
             "<div class=\"nav\"><div class=\"brand\"><div class=\"nav-meta\"><span class=\"mini-label\">IP</span><span class=\"mono\" id=\"live-ip\">%s</span><span class=\"mini-label\">Credits</span><span class=\"mono\" id=\"live-credits\">--</span><span class=\"mini-label\">Tools</span><span class=\"mono\" id=\"live-running-tools\">0</span></div><div class=\"brand-main\"><div class=\"glyph\">%s</div><div><div class=\"pill\" id=\"live-stage-chip\">%s</div><h2 id=\"live-navbar-name\">%s</h2></div></div></div><div class=\"tabs\"><button class=\"tab active\" data-tab=\"home\">Home</button><button class=\"tab\" data-tab=\"telemetry\">Telemetry</button><button class=\"tab\" data-tab=\"settings\">Settings</button><button class=\"tab\" data-tab=\"tools\">Tools</button></div></div>",
             snapshot->primary_ip,
             stage_art,
             life_stage,
             snapshot->dragon_name);
    web_ui_append_text(buffer, buffer_size, snippet);

    snprintf(snippet,
             sizeof(snippet),
             "<section class=\"page active\" id=\"home\"><div class=\"hero\"><div class=\"card hero-core\"><div class=\"section-head\"><div><div class=\"pill\">Dragon Chamber</div><h1 id=\"live-title\">%s</h1><div class=\"muted\" id=\"live-mood-action\">Mood %s, action %s, growth stage %u</div></div><div class=\"chip chip-ready\">Dragon Core</div></div><div class=\"hero-ridge\"><div class=\"stage-sigil\" id=\"live-stage-art\">%s</div><div class=\"speech-panel\"><div><div class=\"muted\">Current voice</div><div class=\"speech\" id=\"live-speech\">%s</div></div><div><div class=\"muted\">Hatch progress</div><div class=\"bar\"><div class=\"fill\" id=\"live-hatch-fill\" style=\"width:%u%%\"></div></div></div><div class=\"hero-stats\"><div class=\"hero-stat\"><div class=\"muted\">XP</div><div class=\"stat\" id=\"live-xp\">%.1f</div></div><div class=\"hero-stat\"><div class=\"muted\">Reward</div><div class=\"stat\" id=\"live-reward\">%.2f</div></div><div class=\"hero-stat\"><div class=\"muted\">Display</div><div class=\"stat\" id=\"live-display\">%s</div></div><div class=\"hero-stat\"><div class=\"muted\">Network</div><div class=\"stat\" id=\"live-network\">%s</div></div></div></div></div><div class=\"status-strip\"><div class=\"status-tile\"><div class=\"muted\">Attitude</div><div class=\"stat\" id=\"live-attitude\">%s</div></div><div class=\"status-tile\"><div class=\"muted\">Stage</div><div class=\"stat\" id=\"live-stage-line\">%s Stage %u</div></div><div class=\"status-tile\"><div class=\"muted\">Tool Slots</div><div class=\"stat\" id=\"live-tool-count\">%u</div></div><div class=\"status-tile\"><div class=\"muted\">Running</div><div class=\"stat\" id=\"live-running-tools-main\">0</div></div></div></div><div class=\"stack\"><div class=\"card signal-card\"><div class=\"muted\">Dragon Mood Stack</div><h2>Sass Channel</h2><div class=\"mood-stack\"><div class=\"mood-item\"><span>Mood</span><span class=\"value\" id=\"live-mood-main\">--</span></div><div class=\"mood-item\"><span>Action</span><span class=\"value\" id=\"live-action-main\">--</span></div><div class=\"mood-item\"><span>Confidence</span><span class=\"value\" id=\"live-confidence-main\">--</span></div><div class=\"mood-item\"><span>Advantage</span><span class=\"value\" id=\"live-advantage-main\">--</span></div><div class=\"mood-item\"><span>Entropy</span><span class=\"value\" id=\"live-entropy-main\">--</span></div><div class=\"mood-item\"><span>Credits</span><span class=\"value\" id=\"live-credits-main\">--</span></div></div><div class=\"speech speech-remarks\" id=\"live-remarks\">%s</div><div class=\"muted\">Primary IP <span class=\"mono\" id=\"live-ip-detail\">%s</span></div><div class=\"actions\"><a class=\"tab active\" href=\"/api/status\">View API</a></div></div><div class=\"card signal-card\"><div class=\"muted\">Credits Vault</div><h2>Manage Balance</h2><div style=\"display:flex;align-items:center;gap:16px;margin-bottom:20px\"><div><div class=\"muted\">Current Balance</div><div class=\"stat\" style=\"font-size:42px;color:#e6b450\" id=\"live-credits-display\">--</div></div><div style=\"flex:1;display:flex;flex-direction:column;gap:12px\"><button class=\"tab\" style=\"padding:10px 14px;font-size:14px\" onclick=\"addCredits(10)\">+ 10 ¢</button><button class=\"tab\" style=\"padding:10px 14px;font-size:14px\" onclick=\"addCredits(50)\">+ 50 ¢</button></div><div style=\"flex:1;display:flex;flex-direction:column;gap:12px\"><button class=\"tab\" style=\"padding:10px 14px;font-size:14px\" onclick=\"subtractCredits(10)\">- 10 ¢</button><button class=\"tab\" style=\"padding:10px 14px;font-size:14px\" onclick=\"subtractCredits(50)\">- 50 ¢</button></div></div><div style=\"display:none;padding:12px;background:var(--panel-2);border-radius:12px;border:1px solid var(--accent);margin-bottom:12px\" id=\"credits-editor\"><div style=\"display:flex;gap:10px;margin-bottom:10px\"><input type=\"number\" id=\"credits-input\" placeholder=\"Enter amount\" step=\"0.01\" style=\"flex:1;padding:8px;border-radius:8px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"><button class=\"tab\" onclick=\"setCreditsValue()\" style=\"padding:8px 12px\">Set</button></div><div class=\"muted\">Current value: ¢ <span id=\"live-credits-edit-value\">0.00</span></div></div><div class=\"actions\"><button class=\"tab\" onclick=\"showCreditsEditor()\" style=\"font-size:13px\">Manual Entry</button></div></div><div class=\"card signal-card\"><div class=\"muted\">Scout Banner</div><h2>Discovery Feed</h2><div class=\"speech\" id=\"live-discovery\">%s</div><div class=\"pulse-grid\"><div><div class=\"muted\">Known Networks</div><div class=\"stat\" id=\"live-known-count\">%u</div></div><div><div class=\"muted\">Nearby Now</div><div class=\"stat\" id=\"live-nearby-count\">%u</div></div></div><div class=\"muted\" style=\"margin-top:12px\">Standalone mode %s, announce new networks %s</div><div class=\"actions\"><button class=\"tab\" type=\"button\" onclick=\"resetNetworkMemory()\">Reset Network Memory</button></div></div><div class=\"card signal-card\"><div class=\"muted\">GPS</div><h2 id=\"live-gps-fix\">%s</h2><div class=\"subgrid\"><div><div class=\"muted\">Lat</div><div class=\"stat mono\" id=\"live-gps-lat\">%s</div></div><div><div class=\"muted\">Long</div><div class=\"stat mono\" id=\"live-gps-long\">%s</div></div><div><div class=\"muted\">Alt</div><div class=\"stat mono\" id=\"live-gps-alt\">%s</div></div></div><div class=\"muted\" style=\"margin-top:12px\">Source <span class=\"mono\" id=\"live-gps-source\">%s</span></div></div><div class=\"card signal-card\"><div class=\"muted\">Nearby Wi-Fi</div><h2><span id=\"live-nearby-count-copy\">%u</span> seen</h2><div class=\"speech\" id=\"live-nearby-list\">%s</div></div><div class=\"card signal-card\"><div class=\"muted\">Runtime</div><div class=\"stat\" id=\"live-heartbeat\">%u</div><div class=\"muted\">Phase <span id=\"live-phase\">%u ms</span></div><div class=\"subgrid\"><div><div class=\"muted\">Confidence</div><div class=\"stat\" id=\"live-confidence\">%.2f</div></div><div><div class=\"muted\">Advantage</div><div class=\"stat\" id=\"live-advantage\">%.2f</div></div></div><div class=\"muted\" style=\"margin-top:14px\">Scan window <span id=\"live-scan\">%u / %u ms</span></div><div class=\"muted\" style=\"margin-top:10px\">Web UI <span class=\"mono\">%s:%u</span></div></div></div></div><div class=\"grid\"><div class=\"card\"><div class=\"muted\">Energy</div><div class=\"bar\"><div class=\"fill\" id=\"live-energy-fill\" style=\"width:%u%%\"></div></div></div><div class=\"card\"><div class=\"muted\">Curiosity</div><div class=\"bar\"><div class=\"fill\" id=\"live-curiosity-fill\" style=\"width:%u%%\"></div></div></div><div class=\"card\"><div class=\"muted\">Bond</div><div class=\"bar\"><div class=\"fill\" id=\"live-bond-fill\" style=\"width:%u%%\"></div></div></div><div class=\"card\"><div class=\"muted\">Reserve</div><div class=\"bar\"><div class=\"fill\" id=\"live-hunger-fill\" style=\"width:%u%%\"></div></div></div></div></section>",
             snapshot->dragon_name,
             mood,
             action,
             snapshot->dragon.growth_stage,
             stage_art,
             snapshot->dragon.speech,
             hatch_progress,
             snapshot->dragon.xp,
             snapshot->dragon.last_reward,
             display_state,
             network_state,
             mood,
             life_stage,
             snapshot->dragon.growth_stage,
             (unsigned int)snapshot->tool_count,
             snapshot->dragon.speech,
             snapshot->primary_ip,
             snapshot->discovery_message[0] != '\0' ? snapshot->discovery_message : "Waiting for new networks",
             snapshot->known_network_total,
             snapshot->nearby_network_count,
             snapshot->standalone_mode ? "on" : "off",
             snapshot->announce_new_networks ? "on" : "off",
             gps_fix,
             gps_lat,
             gps_long,
             gps_alt,
             snapshot->gps_source,
             snapshot->nearby_network_count,
             snapshot->nearby_networks,
             snapshot->heartbeat_ticks,
             snapshot->network_phase_ms,
             snapshot->dragon.policy_confidence,
             snapshot->dragon.last_advantage,
             snapshot->network_phase_ms,
             snapshot->scan_interval_ms,
             snapshot->bind_address,
             snapshot->port,
             energy_pct,
             curiosity_pct,
             bond_pct,
             hunger_pct);
    web_ui_append_text(buffer, buffer_size, snippet);

    snprintf(snippet,
             sizeof(snippet),
             "<section class=\"page\" id=\"telemetry\"><div class=\"grid\"><div class=\"card\"><h2>Learning Telemetry</h2><div class=\"metric\"><span class=\"muted\">Value Estimate</span><strong id=\"live-value\">%.2f</strong></div><div class=\"metric\"><span class=\"muted\">Policy Entropy</span><strong id=\"live-entropy\">%.3f</strong></div><div class=\"metric\"><span class=\"muted\">Reward Total</span><strong id=\"live-reward-total\">%.2f</strong></div><div class=\"metric\"><span class=\"muted\">Reward Average</span><strong id=\"live-reward-average\">%.3f</strong></div><div class=\"metric\"><span class=\"muted\">Reward Events</span><strong id=\"live-reward-events\">%u</strong></div></div><div class=\"card\"><h2>Runtime Shape</h2><div class=\"metric\"><span class=\"muted\">Tick Interval</span><strong>%u ms</strong></div><div class=\"metric\"><span class=\"muted\">Actor Rate</span><strong>%.3f</strong></div><div class=\"metric\"><span class=\"muted\">Critic Rate</span><strong>%.3f</strong></div><div class=\"metric\"><span class=\"muted\">Discount</span><strong>%.3f</strong></div><div class=\"metric\"><span class=\"muted\">Tools Registered</span><strong id=\"live-tool-count-detail\">%u</strong></div></div><div class=\"card\"><h2>Dragon State</h2><div class=\"metric\"><span class=\"muted\">Current Mood</span><strong>%s</strong></div><div class=\"metric\"><span class=\"muted\">Current Action</span><strong>%s</strong></div><div class=\"metric\"><span class=\"muted\">Preferred SSID</span><strong class=\"mono\">%s</strong></div><div class=\"metric\"><span class=\"muted\">Display Backend</span><strong class=\"mono\">%s</strong></div><div class=\"metric\"><span class=\"muted\">Framebuffer</span><strong class=\"mono\">%s</strong></div><div class=\"metric\"><span class=\"muted\">Discovery Hold</span><strong>%u ms</strong></div><div class=\"metric\"><span class=\"muted\">Known Network Memory</span><strong id=\"live-known-count-telemetry\">%u</strong></div></div></div></section>",
             snapshot->dragon.value_estimate,
             snapshot->dragon.policy_entropy,
             snapshot->dragon.reward_total,
             snapshot->dragon.reward_average,
             snapshot->dragon.reward_events,
             snapshot->ai_tick_interval_ms,
             snapshot->ai_actor_learning_rate,
             snapshot->ai_critic_learning_rate,
             snapshot->ai_discount_factor,
             (unsigned int)snapshot->tool_count,
             mood,
             action,
             snapshot->preferred_ssid[0] != '\0' ? snapshot->preferred_ssid : "(none)",
             snapshot->display_backend,
             snapshot->framebuffer_device,
             snapshot->discovery_hold_ms,
             snapshot->known_network_total);
    web_ui_append_text(buffer, buffer_size, snippet);

    snprintf(snippet,
             sizeof(snippet),
             "<section class=\"page\" id=\"settings\"><form method=\"post\" action=\"/settings\"><div class=\"grid\"><div class=\"card\"><div class=\"section-head\"><h2>Identity</h2><span class=\"pill\">Core</span></div><div class=\"grid\"><div><div class=\"muted\">Loki Name</div><input name=\"dragon_name\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">System Label</div><input name=\"device_name\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Hostname</div><input name=\"hostname\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Preferred SSID</div><input name=\"preferred_ssid\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div></div><div class=\"settings-note\">Identity changes save immediately. Plugin and display backend changes may need a restart to fully apply.</div><div class=\"grid\" style=\"margin-top:18px\"><label class=\"muted\"><input type=\"checkbox\" name=\"dhcp_enabled\" %s> DHCP roaming enabled</label><label class=\"muted\"><input type=\"checkbox\" name=\"standalone_mode\" %s> Standalone scout mode</label><label class=\"muted\"><input type=\"checkbox\" name=\"announce_new_networks\" %s> Announce new networks</label><label class=\"muted\"><input type=\"checkbox\" name=\"web_ui_enabled\" %s> Web UI enabled</label><label class=\"muted\"><input type=\"checkbox\" name=\"plugin_display\" %s> Display plugin</label><label class=\"muted\"><input type=\"checkbox\" name=\"plugin_dragon_ai\" %s> Brain plugin</label><label class=\"muted\"><input type=\"checkbox\" name=\"plugin_network_state\" %s> Network plugin</label></div></div><div class=\"card\"><div class=\"section-head\"><h2>Runtime</h2><span class=\"pill\">Display</span></div><div class=\"grid\"><div><div class=\"muted\">Scan Interval</div><input name=\"scan_interval_ms\" value=\"%u\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Hop Interval</div><input name=\"hop_interval_ms\" value=\"%u\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Discovery Hold</div><input name=\"discovery_hold_ms\" value=\"%u\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Web Bind Address</div><input name=\"web_bind_address\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Web Port</div><input name=\"web_port\" value=\"%u\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Display Backend</div><input name=\"display_backend\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Framebuffer Device</div><input name=\"framebuffer_device\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div></div><div class=\"grid\"><label class=\"muted\"><input type=\"checkbox\" name=\"test_tft\" %s> TFT test</label><label class=\"muted\"><input type=\"checkbox\" name=\"test_flash\" %s> Flash test</label><label class=\"muted\"><input type=\"checkbox\" name=\"test_eeprom\" %s> EEPROM test</label><label class=\"muted\"><input type=\"checkbox\" name=\"test_flipper\" %s> Flipper test</label></div></div><div class=\"card\"><div class=\"section-head\"><h2>Brain</h2><span class=\"pill\">A2C</span></div><div class=\"grid\"><div><div class=\"muted\">Learning Rate</div><input name=\"learning_rate\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Actor Rate</div><input name=\"actor_learning_rate\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Critic Rate</div><input name=\"critic_learning_rate\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Discount</div><input name=\"discount_factor\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Reward Decay</div><input name=\"reward_decay\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Entropy Beta</div><input name=\"entropy_beta\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Policy Temperature</div><input name=\"policy_temperature\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Tick Interval</div><input name=\"tick_interval_ms\" value=\"%u\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div></div><div class=\"settings-note\">This is the quick-learning section. Higher actor/critic rates and lower tick intervals make the dragon adapt faster.</div></div><div class=\"card\"><div class=\"section-head\"><h2>Dragon Profile</h2><span class=\"pill\">Behavior</span></div><div class=\"grid\"><div><div class=\"muted\">Profile</div><input name=\"profile\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Temperament</div><input name=\"temperament\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Base Energy</div><input name=\"base_energy\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Base Curiosity</div><input name=\"base_curiosity\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Base Bond</div><input name=\"base_bond\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Base Hunger</div><input name=\"base_hunger\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Hunger Rate</div><input name=\"hunger_rate\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Energy Decay</div><input name=\"energy_decay\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Curiosity Rate</div><input name=\"curiosity_rate\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div></div></div><div class=\"card\"><div class=\"section-head\"><h2>Growth Pace</h2><span class=\"pill\">Slow Grow</span></div><div class=\"grid\"><div><div class=\"muted\">Explore XP Gain</div><input name=\"explore_xp_gain\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Play XP Gain</div><input name=\"play_xp_gain\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Growth Interval</div><input name=\"growth_interval_ms\" value=\"%u\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Growth XP Step</div><input name=\"growth_xp_step\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Egg Stage Max</div><input name=\"egg_stage_max\" value=\"%u\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Hatchling Stage Max</div><input name=\"hatchling_stage_max\" value=\"%u\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div></div></div><div class=\"card\"><div class=\"section-head\"><h2>Add Tool</h2><span class=\"pill\">Utility Deck</span></div><div class=\"grid\"><div><div class=\"muted\">Section Name</div><input name=\"new_tool_section\" placeholder=\"tool_custom_scan\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Display Name</div><input name=\"new_tool_name\" placeholder=\"Passive Scanner\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Command</div><input name=\"new_tool_command\" placeholder=\"raw iwgetid (runs directly)\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div></div><div class=\"muted\" style=\"margin-top:12px\">Description</div><textarea name=\"new_tool_description\" style=\"width:100%%;min-height:72px;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></textarea><div class=\"grid\" style=\"margin-top:12px\"><label class=\"muted\"><input type=\"checkbox\" name=\"new_tool_enabled\" checked> Enable new tool immediately</label></div><div class=\"settings-note\">Commands run directly with no safety checks. Bettercap is not integrated here.</div></div>%s</div><p style=\"margin-top:18px\"><button class=\"tab active\" type=\"submit\">Save Settings</button></p></form></section>",
             snapshot->dragon_name,
             snapshot->device_name,
             snapshot->hostname,
             snapshot->preferred_ssid,
             snapshot->dhcp_enabled ? "checked" : "",
             snapshot->standalone_mode ? "checked" : "",
             snapshot->announce_new_networks ? "checked" : "",
             snapshot->web_ui_enabled ? "checked" : "",
             snapshot->plugin_display ? "checked" : "",
             snapshot->plugin_dragon_ai ? "checked" : "",
             snapshot->plugin_network_state ? "checked" : "",
             snapshot->scan_interval_ms,
             snapshot->hop_interval_ms,
             snapshot->discovery_hold_ms,
             snapshot->bind_address,
             snapshot->port,
             snapshot->display_backend,
             snapshot->framebuffer_device,
             snapshot->test_tft ? "checked" : "",
             snapshot->test_flash ? "checked" : "",
             snapshot->test_eeprom ? "checked" : "",
             snapshot->test_flipper ? "checked" : "",
             snapshot->ai_learning_rate,
             snapshot->ai_actor_learning_rate,
             snapshot->ai_critic_learning_rate,
             snapshot->ai_discount_factor,
             snapshot->ai_reward_decay,
             snapshot->ai_entropy_beta,
             snapshot->ai_policy_temperature,
             snapshot->ai_tick_interval_ms,
             snapshot->dragon_profile,
             snapshot->dragon_temperament,
             snapshot->dragon_base_energy,
             snapshot->dragon_base_curiosity,
             snapshot->dragon_base_bond,
             snapshot->dragon_base_hunger,
             snapshot->dragon_hunger_rate,
             snapshot->dragon_energy_decay,
             snapshot->dragon_curiosity_rate,
             snapshot->dragon_explore_xp_gain,
             snapshot->dragon_play_xp_gain,
             snapshot->dragon_growth_interval_ms,
             snapshot->dragon_growth_xp_step,
             snapshot->dragon_egg_stage_max,
             snapshot->dragon_hatchling_stage_max,
             tool_forms);
    web_ui_append_text(buffer, buffer_size, snippet);

    snprintf(snippet,
             sizeof(snippet),
             "<section class=\"page\" id=\"tools\"><div class=\"loot-grid\"><div class=\"loot-card\"><div class=\"chip\">Growth Cache</div><div class=\"stat\">%u trophies</div><div class=\"muted\">Each growth stage banks one milestone trophy toward adulthood.</div></div><div class=\"loot-card\"><div class=\"chip\">Field Scans</div><div class=\"stat\">%u runs</div><div class=\"muted\">Derived from scheduler heartbeats and scan cadence.</div></div><div class=\"loot-card\"><div class=\"chip\">Bond Ledger</div><div class=\"stat\">%u%% affinity</div><div class=\"muted\">Tool cards below can be run directly, and successful runs now feed the learning loop.</div></div>%s</div></section></div><div id=\"loki-dragon\" class=\"loki-dragon\"></div><div id=\"dragon-chat\" class=\"dragon-chat\">Loki is scanning the mountain horizon...</div></body></html>",
             loot_trophies,
             loot_scans,
             bond_pct,
             tool_cards);
    web_ui_append_text(buffer, buffer_size, snippet);

    free(tool_cards);
    free(tool_forms);
    return buffer;
}

static void web_ui_handle_client(web_ui_t *web_ui, int client_fd)
{
    char request[16384];
    char method[8];
    char path[256];
    ssize_t bytes_read;
    web_ui_snapshot_t snapshot;
    char body[32768];
    int status_code;
    char *html_body;
    char *form_body;
    char remote_address[64];
    bool request_is_localhost;

    bytes_read = recv(client_fd, request, sizeof(request) - 1, 0);
    if (bytes_read <= 0) {
        return;
    }

    request[bytes_read] = '\0';
    method[0] = '\0';
    path[0] = '\0';
    (void)sscanf(request, "%7s %255s", method, path);

    memset(&snapshot, 0, sizeof(snapshot));
    pthread_mutex_lock((web_ui_mutex_t *)web_ui->mutex_handle);
    if (web_ui_clone_snapshot(&snapshot, &web_ui->snapshot) != HAL_OK) {
        pthread_mutex_unlock((web_ui_mutex_t *)web_ui->mutex_handle);
        web_ui_send_message_page(client_fd, "Web UI", "Failed to prepare dashboard snapshot.");
        return;
    }
    pthread_mutex_unlock((web_ui_mutex_t *)web_ui->mutex_handle);

    form_body = strstr(request, "\r\n\r\n");
    if (form_body != NULL) {
        form_body += 4;
    }

    request_is_localhost = web_ui_request_is_localhost(client_fd, remote_address, sizeof(remote_address));

    if (strcmp(method, "OPTIONS") == 0) {
        web_ui_send_status_response(client_fd, 204, "No Content", "text/plain; charset=utf-8", "");
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/settings") == 0) {
        char message[512];
        if (form_body == NULL || web_ui->save_settings == NULL) {
            web_ui_send_message_page(client_fd, "Settings Update", "Settings handler is unavailable.");
            web_ui_free_snapshot_tools(&snapshot);
            return;
        }
        if (web_ui->save_settings(form_body,
                                  request_is_localhost,
                                  remote_address,
                                  message,
                                  sizeof(message),
                                  web_ui->handler_context) == HAL_OK) {
            web_ui_send_message_page(client_fd, "Settings Saved", message);
        } else {
            web_ui_send_message_page(client_fd, "Settings Error", message);
        }
    } else if (strcmp(method, "POST") == 0 && strncmp(path, "/tool/run?id=", 13) == 0) {
        char output[4096];
        size_t tool_index = (size_t)strtoul(path + 13, NULL, 10);
        if (tool_index == 0U || tool_index > snapshot.tool_count || web_ui->run_tool == NULL) {
            web_ui_send_message_page(client_fd, "Tool Runner", "Tool request is invalid.");
            web_ui_free_snapshot_tools(&snapshot);
            return;
        }
        if (web_ui->run_tool(tool_index - 1U,
                             request_is_localhost,
                             remote_address,
                             output,
                             sizeof(output),
                             web_ui->handler_context) == HAL_OK) {
            web_ui_send_message_page(client_fd, "Tool Output", output);
        } else {
            web_ui_send_message_page(client_fd, "Tool Output", output);
        }
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/status") == 0) {
        web_ui_build_json(&snapshot, body, sizeof(body));
        web_ui_send_response(client_fd, "application/json; charset=utf-8", body);
    } else if (strncmp(path, "/api/", 5) == 0) {
        if (web_ui->api_request == NULL) {
            web_ui_send_not_found(client_fd);
        } else {
            status_code = 500;
            body[0] = '\0';
            switch (web_ui->api_request(method,
                                        path,
                                        form_body,
                                        request_is_localhost,
                                        remote_address,
                                        body,
                                        sizeof(body),
                                        web_ui->handler_context)) {
                case HAL_OK:
                    status_code = 200;
                    break;
                case HAL_INVALID_PARAM:
                    status_code = 400;
                    break;
                case HAL_NOT_READY:
                    status_code = 503;
                    break;
                case HAL_BUSY:
                    status_code = 409;
                    break;
                case HAL_NOT_SUPPORTED:
                    status_code = 404;
                    break;
                default:
                    status_code = 500;
                    break;
            }
            web_ui_send_status_response(client_fd,
                                        status_code,
                                        (status_code == 200) ? "OK" :
                                        (status_code == 400) ? "Bad Request" :
                                        (status_code == 404) ? "Not Found" :
                                        (status_code == 409) ? "Conflict" :
                                        (status_code == 503) ? "Service Unavailable" : "Internal Server Error",
                                        "application/json; charset=utf-8",
                                        body[0] != '\0' ? body : "{\"error\":\"request failed\"}");
        }
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
        html_body = web_ui_build_html(&snapshot);
        if (html_body == NULL) {
            web_ui_send_message_page(client_fd, "Web UI", "Failed to render dashboard.");
        } else {
            web_ui_send_response(client_fd, "text/html; charset=utf-8", html_body);
            free(html_body);
        }
    } else {
        web_ui_send_not_found(client_fd);
    }

    web_ui_free_snapshot_tools(&snapshot);
}

static bool web_ui_request_is_localhost(int client_fd, char *remote_address, size_t remote_address_size)
{
    struct sockaddr_storage peer_address;
    socklen_t peer_address_length = sizeof(peer_address);

    web_ui_copy_string(remote_address, remote_address_size, "unknown");
    if (getpeername(client_fd, (struct sockaddr *)&peer_address, &peer_address_length) != 0) {
        return false;
    }

    if (peer_address.ss_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)&peer_address;
        const char *converted = inet_ntop(AF_INET, &ipv4->sin_addr, remote_address, (socklen_t)remote_address_size);
        if (converted == NULL) {
            web_ui_copy_string(remote_address, remote_address_size, "unknown");
        }
        return ntohl(ipv4->sin_addr.s_addr) == INADDR_LOOPBACK;
    }

    if (peer_address.ss_family == AF_INET6) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&peer_address;
        static const struct in6_addr loopback = IN6ADDR_LOOPBACK_INIT;
        const char *converted = inet_ntop(AF_INET6, &ipv6->sin6_addr, remote_address, (socklen_t)remote_address_size);
        if (converted == NULL) {
            web_ui_copy_string(remote_address, remote_address_size, "unknown");
        }
        return memcmp(&ipv6->sin6_addr, &loopback, sizeof(loopback)) == 0;
    }

    return false;
}

void web_ui_set_handlers(web_ui_t *web_ui,
                         web_ui_save_settings_fn save_settings,
                         web_ui_run_tool_fn run_tool,
                         web_ui_api_request_fn api_request,
                         void *context)
{
    if (web_ui == NULL) {
        return;
    }

    web_ui->save_settings = save_settings;
    web_ui->run_tool = run_tool;
    web_ui->api_request = api_request;
    web_ui->handler_context = context;
}

static void *web_ui_thread_main(void *arg)
{
    web_ui_t *web_ui = (web_ui_t *)arg;

    while (web_ui->running) {
        fd_set read_set;
        struct timeval timeout;
        int select_result;

        FD_ZERO(&read_set);
        FD_SET(web_ui->listen_fd, &read_set);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        select_result = select(web_ui->listen_fd + 1, &read_set, NULL, NULL, &timeout);
        if (select_result < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_WARN("Web UI select failed: %d", errno);
            continue;
        }

        if (select_result > 0 && FD_ISSET(web_ui->listen_fd, &read_set)) {
            int client_fd = accept(web_ui->listen_fd, NULL, NULL);
            if (client_fd >= 0) {
                web_ui_handle_client(web_ui, client_fd);
                close(client_fd);
            }
        }
    }

    return NULL;
}

hal_status_t web_ui_init(web_ui_t *web_ui, const char *bind_address, uint16_t port)
{
    if (web_ui == NULL || bind_address == NULL || port == 0U) {
        return HAL_INVALID_PARAM;
    }

    memset(web_ui, 0, sizeof(*web_ui));
    web_ui->port = port;
    web_ui_copy_string(web_ui->bind_address, sizeof(web_ui->bind_address), bind_address);
    web_ui->listen_fd = -1;
    web_ui->mutex_handle = malloc(sizeof(web_ui_mutex_t));
    if (web_ui->mutex_handle == NULL) {
        return HAL_ERROR;
    }

    pthread_mutex_init((web_ui_mutex_t *)web_ui->mutex_handle, NULL);
    return HAL_OK;
}

hal_status_t web_ui_start(web_ui_t *web_ui)
{
    struct sockaddr_in address;
    int option_value = 1;
    bool fallback_address = false;
    bool fallback_port = false;
    web_ui_thread_t *thread_handle;

    if (web_ui == NULL) {
        return HAL_INVALID_PARAM;
    }

    web_ui->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (web_ui->listen_fd < 0) {
        LOG_WARN("Web UI socket creation failed");
        return HAL_ERROR;
    }

    setsockopt(web_ui->listen_fd, SOL_SOCKET, SO_REUSEADDR, &option_value, sizeof(option_value));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(web_ui->port);
    if (inet_pton(AF_INET, web_ui->bind_address, &address.sin_addr) != 1) {
        LOG_WARN("Web UI bind address is invalid: %s. Falling back to 0.0.0.0", web_ui->bind_address);
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        web_ui_copy_string(web_ui->bind_address, sizeof(web_ui->bind_address), "0.0.0.0");
        fallback_address = true;
    }

    if (bind(web_ui->listen_fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        LOG_WARN("Web UI bind failed on %s:%u, retrying on 0.0.0.0:%u",
                 web_ui->bind_address,
                 web_ui->port,
                 web_ui->port);

        /* Recover from stale static bind IPs by retrying INADDR_ANY on the same port first. */
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(web_ui->listen_fd, (struct sockaddr *)&address, sizeof(address)) == 0) {
            web_ui_copy_string(web_ui->bind_address, sizeof(web_ui->bind_address), "0.0.0.0");
            fallback_address = true;
        } else if (web_ui->port != 8080U) {
            LOG_WARN("Web UI bind retry failed, attempting fallback endpoint 0.0.0.0:8080");
            address.sin_port = htons(8080U);
            if (bind(web_ui->listen_fd, (struct sockaddr *)&address, sizeof(address)) == 0) {
                web_ui_copy_string(web_ui->bind_address, sizeof(web_ui->bind_address), "0.0.0.0");
                web_ui->port = 8080U;
                fallback_address = true;
                fallback_port = true;
            } else {
                LOG_WARN("Web UI bind fallback failed on 0.0.0.0:8080");
                close(web_ui->listen_fd);
                web_ui->listen_fd = -1;
                return HAL_BUSY;
            }
        } else {
            LOG_WARN("Web UI bind fallback failed on 0.0.0.0:%u", web_ui->port);
            close(web_ui->listen_fd);
            web_ui->listen_fd = -1;
            return HAL_BUSY;
        }
    }

    if (listen(web_ui->listen_fd, 4) != 0) {
        close(web_ui->listen_fd);
        web_ui->listen_fd = -1;
        return HAL_ERROR;
    }

    thread_handle = malloc(sizeof(web_ui_thread_t));
    if (thread_handle == NULL) {
        close(web_ui->listen_fd);
        web_ui->listen_fd = -1;
        return HAL_ERROR;
    }

    web_ui->thread_handle = thread_handle;
    web_ui->running = true;
    if (pthread_create(thread_handle, NULL, web_ui_thread_main, web_ui) != 0) {
        web_ui->running = false;
        free(web_ui->thread_handle);
        web_ui->thread_handle = NULL;
        close(web_ui->listen_fd);
        web_ui->listen_fd = -1;
        return HAL_ERROR;
    }

    if (fallback_address || fallback_port) {
        LOG_WARN("Web UI started with fallback endpoint http://%s:%u", web_ui->bind_address, web_ui->port);
    } else {
        LOG_INFO("Web UI listening on http://%s:%u", web_ui->bind_address, web_ui->port);
    }
    return HAL_OK;
}

hal_status_t web_ui_update_snapshot(web_ui_t *web_ui, const web_ui_snapshot_t *snapshot)
{
    web_ui_snapshot_t cloned;

    if (web_ui == NULL || snapshot == NULL || web_ui->mutex_handle == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (web_ui_clone_snapshot(&cloned, snapshot) != HAL_OK) {
        return HAL_ERROR;
    }

    pthread_mutex_lock((web_ui_mutex_t *)web_ui->mutex_handle);
    web_ui_free_snapshot_tools(&web_ui->snapshot);
    web_ui->snapshot = cloned;
    pthread_mutex_unlock((web_ui_mutex_t *)web_ui->mutex_handle);
    return HAL_OK;
}

hal_status_t web_ui_stop(web_ui_t *web_ui)
{
    if (web_ui == NULL) {
        return HAL_INVALID_PARAM;
    }

    web_ui->running = false;
    if (web_ui->listen_fd >= 0) {
        shutdown(web_ui->listen_fd, SHUT_RDWR);
        close(web_ui->listen_fd);
        web_ui->listen_fd = -1;
    }

    if (web_ui->thread_handle != NULL) {
        pthread_join(*(web_ui_thread_t *)web_ui->thread_handle, NULL);
        free(web_ui->thread_handle);
        web_ui->thread_handle = NULL;
    }

    web_ui_free_snapshot_tools(&web_ui->snapshot);

    if (web_ui->mutex_handle != NULL) {
        pthread_mutex_destroy((web_ui_mutex_t *)web_ui->mutex_handle);
        free(web_ui->mutex_handle);
        web_ui->mutex_handle = NULL;
    }

    return HAL_OK;
}

#else

hal_status_t web_ui_init(web_ui_t *web_ui, const char *bind_address, uint16_t port)
{
    (void)bind_address;
    (void)port;
    if (web_ui == NULL) {
        return HAL_INVALID_PARAM;
    }
    memset(web_ui, 0, sizeof(*web_ui));
    return HAL_OK;
}

hal_status_t web_ui_start(web_ui_t *web_ui)
{
    (void)web_ui;
    return HAL_NOT_SUPPORTED;
}

hal_status_t web_ui_update_snapshot(web_ui_t *web_ui, const web_ui_snapshot_t *snapshot)
{
    (void)web_ui;
    (void)snapshot;
    return HAL_OK;
}

hal_status_t web_ui_stop(web_ui_t *web_ui)
{
    (void)web_ui;
    return HAL_OK;
}

#endif
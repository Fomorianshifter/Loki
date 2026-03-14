/**
 * @file web_ui.c
 * @brief Lightweight embedded web UI for Loki.
 */

#include "web_ui.h"

#include <stdio.h>
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

static void web_ui_send_response(int client_fd, const char *content_type, const char *body);

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

static void web_ui_build_tool_cards(const web_ui_snapshot_t *snapshot, char *buffer, size_t buffer_size)
{
    size_t index;

    buffer[0] = '\0';
    for (index = 0; index < snapshot->tool_count; index++) {
        char card[768];
        snprintf(card,
                 sizeof(card),
                 "<div class=\"loot-card\"><div class=\"chip\">%s</div><div class=\"stat\">%s</div><div class=\"muted\">%s</div><div class=\"muted\" style=\"margin-top:12px\">Command: %s</div><form method=\"post\" action=\"/tool/run?id=%u\" style=\"margin-top:14px\"><button class=\"tab active\" type=\"submit\">Run Tool</button></form></div>",
                 snapshot->tools[index].enabled ? "Active" : "Idle",
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
                 snapshot->tools[index].command,
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

static void web_ui_send_response(int client_fd, const char *content_type, const char *body)
{
    char header[256];
    int header_len;
    size_t body_len;

    body_len = strlen(body);
    header_len = snprintf(header,
                          sizeof(header),
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %u\r\n"
                          "Connection: close\r\n\r\n",
                          content_type,
                          (unsigned int)body_len);

    if (header_len > 0) {
        send(client_fd, header, (size_t)header_len, 0);
        send(client_fd, body, body_len, 0);
    }
}

static void web_ui_send_not_found(int client_fd)
{
    static const char *response =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 9\r\n"
        "Connection: close\r\n\r\nNot Found";

    send(client_fd, response, strlen(response), 0);
}

static void web_ui_build_json(const web_ui_snapshot_t *snapshot, char *buffer, size_t buffer_size)
{
    snprintf(buffer,
             buffer_size,
             "{"
             "\"device_name\":\"%s\","
             "\"dragon_name\":\"%s\","
             "\"hostname\":\"%s\","
             "\"primary_ip\":\"%s\","
             "\"bind_address\":\"%s\","
             "\"port\":%u,"
             "\"dhcp_enabled\":%s,"
             "\"display_ready\":%s,"
             "\"preferred_ssid\":\"%s\","
             "\"display_backend\":\"%s\","
             "\"framebuffer_device\":\"%s\","
             "\"tool_count\":%u,"
             "\"heartbeat_ticks\":%u,"
             "\"network_phase_ms\":%u,"
             "\"scan_interval_ms\":%u,"
             "\"hop_interval_ms\":%u,"
             "\"ai\":{"
             "\"learning_rate\":%.3f,"
             "\"actor_learning_rate\":%.3f,"
             "\"critic_learning_rate\":%.3f,"
             "\"discount_factor\":%.3f,"
             "\"reward_decay\":%.3f,"
             "\"entropy_beta\":%.3f,"
             "\"policy_temperature\":%.3f,"
             "\"tick_interval_ms\":%u"
             "},"
             "\"dragon\":{" 
             "\"name\":\"%s\","
             "\"age_ticks\":%u,"
             "\"growth_stage\":%u,"
             "\"life_stage\":\"%s\","
             "\"hunger\":%.3f,"
             "\"energy\":%.3f,"
             "\"curiosity\":%.3f,"
             "\"bond\":%.3f,"
             "\"xp\":%.3f,"
             "\"last_reward\":%.3f,"
             "\"last_advantage\":%.3f,"
             "\"policy_confidence\":%.3f,"
             "\"policy_entropy\":%.3f,"
             "\"value_estimate\":%.3f,"
             "\"reward_total\":%.3f,"
             "\"reward_average\":%.3f,"
             "\"reward_events\":%u,"
             "\"speech\":\"%s\","
             "\"mood\":\"%s\","
             "\"action\":\"%s\""
             "}"
             "}",
             snapshot->device_name,
             snapshot->dragon_name,
             snapshot->hostname,
             snapshot->primary_ip,
             snapshot->bind_address,
             snapshot->port,
             snapshot->dhcp_enabled ? "true" : "false",
             snapshot->display_ready ? "true" : "false",
             snapshot->preferred_ssid,
             snapshot->display_backend,
             snapshot->framebuffer_device,
             (unsigned int)snapshot->tool_count,
             snapshot->heartbeat_ticks,
             snapshot->network_phase_ms,
             snapshot->scan_interval_ms,
             snapshot->hop_interval_ms,
             snapshot->ai_learning_rate,
             snapshot->ai_actor_learning_rate,
             snapshot->ai_critic_learning_rate,
             snapshot->ai_discount_factor,
             snapshot->ai_reward_decay,
             snapshot->ai_entropy_beta,
             snapshot->ai_policy_temperature,
             snapshot->ai_tick_interval_ms,
             snapshot->dragon.name,
             snapshot->dragon.age_ticks,
             snapshot->dragon.growth_stage,
             web_ui_life_stage_name(snapshot->dragon.growth_stage),
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
             snapshot->dragon.reward_events,
             snapshot->dragon.speech,
             web_ui_mood_name(snapshot->dragon.mood),
             web_ui_action_name(snapshot->dragon.action));
}

static char *web_ui_build_html(const web_ui_snapshot_t *snapshot)
{
    const char *display_state = snapshot->display_ready ? "ready" : "offline";
    const char *network_state = snapshot->dhcp_enabled ? "dhcp" : "static";
    const char *mood = web_ui_mood_name(snapshot->dragon.mood);
    const char *action = web_ui_action_name(snapshot->dragon.action);
    const char *life_stage = web_ui_life_stage_name(snapshot->dragon.growth_stage);
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
    char snippet[12288];
    size_t tool_cards_size;
    size_t tool_forms_size;
    size_t buffer_size;

    if (dragon_ai_life_stage(snapshot->dragon.growth_stage) == DRAGON_LIFE_STAGE_HATCHLING) {
        stage_art = "🐉";
    } else if (dragon_ai_life_stage(snapshot->dragon.growth_stage) == DRAGON_LIFE_STAGE_ADULT) {
        stage_art = "🐲";
    }

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
                       ":root{color-scheme:dark;--bg:#07141d;--panel:#10202c;--panel-2:#152b39;--line:rgba(173,226,220,.12);--ink:#edf7f5;--muted:#96b6ba;--accent:#8ce9c8;--accent-2:#ffbf73;--danger:#ff7979;}"
                       "body{margin:0;font-family:Verdana,sans-serif;background:radial-gradient(circle at top,#163445 0,#0c1d29 45%,#07141d 100%);color:var(--ink);}"
                       ".wrap{max-width:1120px;margin:0 auto;padding:22px;}"
                       ".nav{display:flex;align-items:center;justify-content:space-between;gap:16px;padding:14px 18px;background:rgba(10,21,30,.82);border:1px solid var(--line);border-radius:18px;backdrop-filter:blur(14px);position:sticky;top:12px;z-index:3;}"
                       ".brand{display:flex;align-items:center;gap:14px;}.glyph{width:48px;height:48px;border-radius:16px;display:grid;place-items:center;background:linear-gradient(135deg,#163c51,#244d46);font-size:24px;}"
                       ".tabs{display:flex;gap:10px;flex-wrap:wrap;}.tab{padding:10px 14px;border-radius:999px;border:1px solid var(--line);background:transparent;color:var(--muted);cursor:pointer;font-size:13px;text-transform:uppercase;letter-spacing:.08em;}"
                       ".tab.active{background:var(--accent);color:#09202a;border-color:transparent;font-weight:700;}.page{display:none;padding-top:18px;}.page.active{display:block;}"
                       ".hero{display:grid;grid-template-columns:1.7fr 1fr;gap:18px;}.card{background:linear-gradient(180deg,rgba(16,32,44,.95),rgba(13,25,36,.88));border:1px solid var(--line);border-radius:22px;padding:20px;box-shadow:0 18px 50px rgba(0,0,0,.24);}"
                       "h1{margin:0 0 8px;font-size:42px;letter-spacing:.03em;}h2{margin:0;font-size:24px;}.muted{color:var(--muted);font-size:14px;}"
                       ".metric{display:flex;justify-content:space-between;padding:12px 0;border-bottom:1px solid rgba(255,255,255,.08);gap:12px;}.metric:last-child{border-bottom:none;}"
                       ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:18px;margin-top:18px;}.pill{display:inline-block;padding:7px 12px;border-radius:999px;background:#18384d;color:var(--accent);text-transform:uppercase;font-size:12px;letter-spacing:.08em;}"
                       ".big-stage{font-size:78px;line-height:1;margin:14px 0;}.bar{height:12px;background:#0d2030;border-radius:999px;overflow:hidden;margin-top:8px;}.fill{height:100%;background:linear-gradient(90deg,var(--accent-2),var(--accent));}"
                       ".stat{font-size:28px;font-weight:700;margin-top:8px;}.stack{display:grid;gap:14px;}.subgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin-top:16px;}"
                       ".loot-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:18px;}.loot-card{padding:20px;border-radius:20px;background:linear-gradient(180deg,rgba(26,33,46,.96),rgba(17,24,35,.88));border:1px solid rgba(255,190,117,.14);}"
                       ".chip{display:inline-flex;gap:8px;align-items:center;padding:8px 12px;border-radius:999px;background:rgba(255,191,115,.1);color:var(--accent-2);font-size:12px;text-transform:uppercase;letter-spacing:.08em;}"
                       ".speech{margin-top:14px;padding:14px 16px;border-radius:16px;background:#0b1721;border:1px solid var(--line);font-size:15px;line-height:1.5;min-height:52px;}"
                       ".status-strip{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin-top:18px;}.status-tile{padding:14px 16px;border-radius:16px;background:#0b1721;border:1px solid var(--line);}"
                       ".mono{font-family:Consolas,monospace;}.actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:14px;}input,textarea{box-sizing:border-box;}a{color:#9ae7ff;text-decoration:none;}"
                       "@media (max-width:860px){.hero{grid-template-columns:1fr;}.nav{flex-direction:column;align-items:flex-start;}}</style>"
                       "<script>"
                       "function showTab(tab){document.querySelectorAll('.tab').forEach(function(el){el.classList.toggle('active',el.dataset.tab===tab);});document.querySelectorAll('.page').forEach(function(el){el.classList.toggle('active',el.id===tab);});}"
                       "function setText(id,value){var el=document.getElementById(id);if(el){el.textContent=String(value);}}"
                       "function setWidth(id,value){var el=document.getElementById(id);if(el){el.style.width=Math.max(0,Math.min(100,value))+'%';}}"
                       "function stageGlyph(stage){if(stage==='Adult'){return '🐲';}if(stage==='Hatchling'){return '🐉';}return '🥚';}"
                       "async function refreshStatus(){try{var response=await fetch('/api/status',{cache:'no-store'});if(!response.ok){return;}var data=await response.json();setText('live-navbar-name',data.dragon_name);setText('live-stage-chip',String(data.dragon.life_stage).toUpperCase());setText('live-title',data.dragon_name);setText('live-mood-action','Mood '+data.dragon.mood+', action '+data.dragon.action+', growth stage '+data.dragon.growth_stage);setText('live-stage-art',stageGlyph(data.dragon.life_stage));setText('live-stage-line',data.dragon.life_stage+' Stage '+data.dragon.growth_stage);setText('live-xp',Number(data.dragon.xp).toFixed(1));setText('live-reward',Number(data.dragon.last_reward).toFixed(2));setText('live-display',data.display_ready?'ready':'offline');setText('live-network',data.dhcp_enabled?'dhcp':'static');setText('live-speech',data.dragon.speech);setText('live-heartbeat',data.heartbeat_ticks);setText('live-phase',data.network_phase_ms+' ms');setText('live-confidence',Number(data.dragon.policy_confidence).toFixed(2));setText('live-advantage',Number(data.dragon.last_advantage).toFixed(2));setText('live-value',Number(data.dragon.value_estimate).toFixed(2));setText('live-entropy',Number(data.dragon.policy_entropy).toFixed(3));setText('live-reward-total',Number(data.dragon.reward_total).toFixed(2));setText('live-reward-average',Number(data.dragon.reward_average).toFixed(3));setText('live-reward-events',data.dragon.reward_events);setText('live-tool-count',data.tool_count);setText('live-tool-count-detail',data.tool_count);setText('live-ip',data.primary_ip);setText('live-ip-detail',data.primary_ip);setText('live-scan',data.network_phase_ms+' / '+data.scan_interval_ms+' ms');setWidth('live-hatch-fill',data.dragon.growth_stage>=4?100:Math.round((data.dragon.growth_stage*100)/4));setWidth('live-energy-fill',Math.round(data.dragon.energy*100));setWidth('live-curiosity-fill',Math.round(data.dragon.curiosity*100));setWidth('live-bond-fill',Math.round(data.dragon.bond*100));setWidth('live-hunger-fill',Math.round((1-data.dragon.hunger)*100));}catch(error){}}"
                       "window.addEventListener('DOMContentLoaded',function(){document.querySelectorAll('.tab').forEach(function(el){el.addEventListener('click',function(){showTab(el.dataset.tab);});});showTab('home');refreshStatus();window.setInterval(refreshStatus,2000);});"
                       "</script></head><body><div class=\"wrap\">");

    snprintf(snippet,
             sizeof(snippet),
             "<div class=\"nav\"><div class=\"brand\"><div class=\"glyph\">%s</div><div><div class=\"pill\" id=\"live-stage-chip\">%s</div><h2 id=\"live-navbar-name\">%s</h2><div class=\"muted\">Primary IP <span class=\"mono\" id=\"live-ip\">%s</span></div></div></div><div class=\"tabs\"><button class=\"tab active\" data-tab=\"home\">Home</button><button class=\"tab\" data-tab=\"telemetry\">Telemetry</button><button class=\"tab\" data-tab=\"settings\">Settings</button><button class=\"tab\" data-tab=\"tools\">Tools</button></div></div>",
             stage_art,
             life_stage,
             snapshot->dragon_name,
             snapshot->primary_ip);
    web_ui_append_text(buffer, buffer_size, snippet);

    snprintf(snippet,
             sizeof(snippet),
             "<section class=\"page active\" id=\"home\"><div class=\"hero\"><div class=\"card\"><div class=\"pill\">Lifecycle</div><h1 id=\"live-title\">%s</h1><div class=\"muted\" id=\"live-mood-action\">Mood %s, action %s, growth stage %u</div><div class=\"big-stage\" id=\"live-stage-art\">%s</div><div class=\"muted\">Hatch progress</div><div class=\"bar\"><div class=\"fill\" id=\"live-hatch-fill\" style=\"width:%u%%\"></div></div><div class=\"grid\"><div><div class=\"muted\">XP</div><div class=\"stat\" id=\"live-xp\">%.1f</div></div><div><div class=\"muted\">Reward</div><div class=\"stat\" id=\"live-reward\">%.2f</div></div><div><div class=\"muted\">Display</div><div class=\"stat\" id=\"live-display\">%s</div></div><div><div class=\"muted\">Network</div><div class=\"stat\" id=\"live-network\">%s</div></div></div><div class=\"speech\" id=\"live-speech\">%s</div><div class=\"status-strip\"><div class=\"status-tile\"><div class=\"muted\">IP Address</div><div class=\"stat mono\" id=\"live-ip-detail\">%s</div></div><div class=\"status-tile\"><div class=\"muted\">Stage</div><div class=\"stat\" id=\"live-stage-line\">%s Stage %u</div></div><div class=\"status-tile\"><div class=\"muted\">Tool Slots</div><div class=\"stat\" id=\"live-tool-count\">%u</div></div></div></div><div class=\"stack\"><div class=\"card\"><div class=\"muted\">Web UI</div><h2 class=\"mono\">%s:%u</h2><div class=\"muted\">API at <a href=\"/api/status\">/api/status</a></div><div class=\"actions\"><a class=\"tab active\" href=\"/api/status\">View API</a></div></div><div class=\"card\"><div class=\"muted\">Runtime</div><div class=\"stat\" id=\"live-heartbeat\">%u</div><div class=\"muted\">Phase <span id=\"live-phase\">%u ms</span></div><div class=\"subgrid\"><div><div class=\"muted\">Confidence</div><div class=\"stat\" id=\"live-confidence\">%.2f</div></div><div><div class=\"muted\">Advantage</div><div class=\"stat\" id=\"live-advantage\">%.2f</div></div></div><div class=\"muted\" style=\"margin-top:14px\">Scan window <span id=\"live-scan\">%u / %u ms</span></div></div></div></div><div class=\"grid\"><div class=\"card\"><div class=\"muted\">Energy</div><div class=\"bar\"><div class=\"fill\" id=\"live-energy-fill\" style=\"width:%u%%\"></div></div></div><div class=\"card\"><div class=\"muted\">Curiosity</div><div class=\"bar\"><div class=\"fill\" id=\"live-curiosity-fill\" style=\"width:%u%%\"></div></div></div><div class=\"card\"><div class=\"muted\">Bond</div><div class=\"bar\"><div class=\"fill\" id=\"live-bond-fill\" style=\"width:%u%%\"></div></div></div><div class=\"card\"><div class=\"muted\">Reserve</div><div class=\"bar\"><div class=\"fill\" id=\"live-hunger-fill\" style=\"width:%u%%\"></div></div></div></div></section>",
             snapshot->dragon_name,
             mood,
             action,
             snapshot->dragon.growth_stage,
             stage_art,
             hatch_progress,
             snapshot->dragon.xp,
             snapshot->dragon.last_reward,
             display_state,
             network_state,
             snapshot->dragon.speech,
             snapshot->primary_ip,
             life_stage,
             snapshot->dragon.growth_stage,
             (unsigned int)snapshot->tool_count,
             snapshot->bind_address,
             snapshot->port,
             snapshot->heartbeat_ticks,
             snapshot->network_phase_ms,
             snapshot->dragon.policy_confidence,
             snapshot->dragon.last_advantage,
             snapshot->network_phase_ms,
             snapshot->scan_interval_ms,
             energy_pct,
             curiosity_pct,
             bond_pct,
             hunger_pct);
    web_ui_append_text(buffer, buffer_size, snippet);

    snprintf(snippet,
             sizeof(snippet),
             "<section class=\"page\" id=\"telemetry\"><div class=\"grid\"><div class=\"card\"><h2>Learning Telemetry</h2><div class=\"metric\"><span class=\"muted\">Value Estimate</span><strong id=\"live-value\">%.2f</strong></div><div class=\"metric\"><span class=\"muted\">Policy Entropy</span><strong id=\"live-entropy\">%.3f</strong></div><div class=\"metric\"><span class=\"muted\">Reward Total</span><strong id=\"live-reward-total\">%.2f</strong></div><div class=\"metric\"><span class=\"muted\">Reward Average</span><strong id=\"live-reward-average\">%.3f</strong></div><div class=\"metric\"><span class=\"muted\">Reward Events</span><strong id=\"live-reward-events\">%u</strong></div></div><div class=\"card\"><h2>Runtime Shape</h2><div class=\"metric\"><span class=\"muted\">Tick Interval</span><strong>%u ms</strong></div><div class=\"metric\"><span class=\"muted\">Actor Rate</span><strong>%.3f</strong></div><div class=\"metric\"><span class=\"muted\">Critic Rate</span><strong>%.3f</strong></div><div class=\"metric\"><span class=\"muted\">Discount</span><strong>%.3f</strong></div><div class=\"metric\"><span class=\"muted\">Tools Registered</span><strong id=\"live-tool-count-detail\">%u</strong></div></div><div class=\"card\"><h2>Dragon State</h2><div class=\"metric\"><span class=\"muted\">Current Mood</span><strong>%s</strong></div><div class=\"metric\"><span class=\"muted\">Current Action</span><strong>%s</strong></div><div class=\"metric\"><span class=\"muted\">Preferred SSID</span><strong class=\"mono\">%s</strong></div><div class=\"metric\"><span class=\"muted\">Display Backend</span><strong class=\"mono\">%s</strong></div><div class=\"metric\"><span class=\"muted\">Framebuffer</span><strong class=\"mono\">%s</strong></div></div></div></section>",
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
             snapshot->framebuffer_device);
    web_ui_append_text(buffer, buffer_size, snippet);

    snprintf(snippet,
             sizeof(snippet),
             "<section class=\"page\" id=\"settings\"><form method=\"post\" action=\"/settings\"><div class=\"grid\"><div class=\"card\"><h2>Identity</h2><div class=\"grid\"><div><div class=\"muted\">Loki Name</div><input name=\"dragon_name\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">System Label</div><input name=\"device_name\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Hostname</div><input name=\"hostname\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Preferred SSID</div><input name=\"preferred_ssid\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div></div><div class=\"muted\" style=\"margin-top:12px\">The dragon is Loki. The system label is only for internal runtime identification.</div><div class=\"grid\" style=\"margin-top:18px\"><label class=\"muted\"><input type=\"checkbox\" name=\"dhcp_enabled\" %s> DHCP roaming enabled</label><label class=\"muted\"><input type=\"checkbox\" name=\"web_ui_enabled\" %s> Web UI enabled</label></div></div><div class=\"card\"><h2>Runtime</h2><div class=\"grid\"><div><div class=\"muted\">Scan Interval</div><input name=\"scan_interval_ms\" value=\"%u\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Hop Interval</div><input name=\"hop_interval_ms\" value=\"%u\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Web Bind Address</div><input name=\"web_bind_address\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Web Port</div><input name=\"web_port\" value=\"%u\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Display Backend</div><input name=\"display_backend\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Framebuffer Device</div><input name=\"framebuffer_device\" value=\"%s\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div></div><div class=\"grid\"><label class=\"muted\"><input type=\"checkbox\" name=\"test_tft\" %s> TFT test</label><label class=\"muted\"><input type=\"checkbox\" name=\"test_flash\" %s> Flash test</label><label class=\"muted\"><input type=\"checkbox\" name=\"test_eeprom\" %s> EEPROM test</label><label class=\"muted\"><input type=\"checkbox\" name=\"test_flipper\" %s> Flipper test</label></div></div><div class=\"card\"><h2>A2C Controls</h2><div class=\"grid\"><div><div class=\"muted\">Learning Rate</div><input name=\"learning_rate\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Actor Rate</div><input name=\"actor_learning_rate\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Critic Rate</div><input name=\"critic_learning_rate\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Discount</div><input name=\"discount_factor\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Reward Decay</div><input name=\"reward_decay\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Entropy Beta</div><input name=\"entropy_beta\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Policy Temperature</div><input name=\"policy_temperature\" value=\"%.3f\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Tick Interval</div><input name=\"tick_interval_ms\" value=\"%u\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div></div><div class=\"muted\" style=\"margin-top:12px\">These values now drive live telemetry and online actor critic updates during runtime.</div></div><div class=\"card\"><h2>Add Tool</h2><div class=\"grid\"><div><div class=\"muted\">Section Name</div><input name=\"new_tool_section\" placeholder=\"tool_custom_scan\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Display Name</div><input name=\"new_tool_name\" placeholder=\"Passive Scanner\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div><div><div class=\"muted\">Command</div><input name=\"new_tool_command\" placeholder=\"internal://status or safe://iwgetid\" style=\"width:100%%;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></div></div><div class=\"muted\" style=\"margin-top:12px\">Description</div><textarea name=\"new_tool_description\" style=\"width:100%%;min-height:72px;padding:10px;border-radius:12px;border:1px solid var(--line);background:#0b1721;color:var(--ink)\"></textarea><div class=\"grid\" style=\"margin-top:12px\"><label class=\"muted\"><input type=\"checkbox\" name=\"new_tool_enabled\" checked> Enable new tool immediately</label></div><div class=\"muted\" style=\"margin-top:12px\">Leave section name blank to auto-build one from the display name.</div></div>%s</div><p style=\"margin-top:18px\"><button class=\"tab active\" type=\"submit\">Save Settings</button></p></form></section>",
             snapshot->dragon_name,
             snapshot->device_name,
             snapshot->hostname,
             snapshot->preferred_ssid,
             snapshot->dhcp_enabled ? "checked" : "",
             snapshot->web_ui_enabled ? "checked" : "",
             snapshot->scan_interval_ms,
             snapshot->hop_interval_ms,
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
             tool_forms);
    web_ui_append_text(buffer, buffer_size, snippet);

    snprintf(snippet,
             sizeof(snippet),
             "<section class=\"page\" id=\"tools\"><div class=\"loot-grid\"><div class=\"loot-card\"><div class=\"chip\">Growth Cache</div><div class=\"stat\">%u trophies</div><div class=\"muted\">Each growth stage banks one milestone trophy toward adulthood.</div></div><div class=\"loot-card\"><div class=\"chip\">Field Scans</div><div class=\"stat\">%u runs</div><div class=\"muted\">Derived from scheduler heartbeats and scan cadence.</div></div><div class=\"loot-card\"><div class=\"chip\">Bond Ledger</div><div class=\"stat\">%u%% affinity</div><div class=\"muted\">Tool cards below can be run directly, and successful runs now feed the learning loop.</div></div>%s</div></section></div></body></html>",
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
    ssize_t bytes_read;
    web_ui_snapshot_t snapshot;
    char body[32768];
    char *html_body;
    char *form_body;

    bytes_read = recv(client_fd, request, sizeof(request) - 1, 0);
    if (bytes_read <= 0) {
        return;
    }

    request[bytes_read] = '\0';

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

    if (strncmp(request, "POST /settings", 14) == 0) {
        char message[512];
        if (form_body == NULL || web_ui->save_settings == NULL) {
            web_ui_send_message_page(client_fd, "Settings Update", "Settings handler is unavailable.");
            web_ui_free_snapshot_tools(&snapshot);
            return;
        }
        if (web_ui->save_settings(form_body, message, sizeof(message), web_ui->handler_context) == HAL_OK) {
            web_ui_send_message_page(client_fd, "Settings Saved", message);
        } else {
            web_ui_send_message_page(client_fd, "Settings Error", message);
        }
    } else if (strncmp(request, "POST /tool/run?id=", 18) == 0) {
        char output[4096];
        size_t tool_index = (size_t)strtoul(request + 18, NULL, 10);
        if (tool_index == 0U || tool_index > snapshot.tool_count || web_ui->run_tool == NULL) {
            web_ui_send_message_page(client_fd, "Tool Runner", "Tool request is invalid.");
            web_ui_free_snapshot_tools(&snapshot);
            return;
        }
        if (web_ui->run_tool(tool_index - 1U, output, sizeof(output), web_ui->handler_context) == HAL_OK) {
            web_ui_send_message_page(client_fd, "Tool Output", output);
        } else {
            web_ui_send_message_page(client_fd, "Tool Output", output);
        }
    } else if (strncmp(request, "GET /api/status", 15) == 0) {
        web_ui_build_json(&snapshot, body, sizeof(body));
        web_ui_send_response(client_fd, "application/json; charset=utf-8", body);
    } else if (strncmp(request, "GET / ", 6) == 0 || strncmp(request, "GET /HTTP", 9) == 0) {
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

void web_ui_set_handlers(web_ui_t *web_ui,
                         web_ui_save_settings_fn save_settings,
                         web_ui_run_tool_fn run_tool,
                         void *context)
{
    if (web_ui == NULL) {
        return;
    }

    web_ui->save_settings = save_settings;
    web_ui->run_tool = run_tool;
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
        close(web_ui->listen_fd);
        web_ui->listen_fd = -1;
        LOG_WARN("Web UI bind address is invalid: %s", web_ui->bind_address);
        return HAL_INVALID_PARAM;
    }

    if (bind(web_ui->listen_fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        LOG_WARN("Web UI bind failed on %s:%u", web_ui->bind_address, web_ui->port);
        close(web_ui->listen_fd);
        web_ui->listen_fd = -1;
        return HAL_BUSY;
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

    LOG_INFO("Web UI listening on http://%s:%u", web_ui->bind_address, web_ui->port);
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
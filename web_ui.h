#ifndef WEB_UI_H
#define WEB_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dragon_ai.h"
#include "loki_config.h"
#include "platform.h"
#include "types.h"

#define WEB_UI_TEXT_MAX 64

typedef struct {
    char device_name[WEB_UI_TEXT_MAX];
    char dragon_name[WEB_UI_TEXT_MAX];
    char hostname[WEB_UI_TEXT_MAX];
    char primary_ip[WEB_UI_TEXT_MAX];
    char preferred_ssid[WEB_UI_TEXT_MAX];
    char bind_address[WEB_UI_TEXT_MAX];
    char display_backend[WEB_UI_TEXT_MAX];
    char framebuffer_device[WEB_UI_TEXT_MAX];
    uint32_t heartbeat_ticks;
    uint32_t network_phase_ms;
    uint32_t scan_interval_ms;
    uint32_t hop_interval_ms;
    uint32_t ai_tick_interval_ms;
    uint16_t port;
    bool dhcp_enabled;
    bool web_ui_enabled;
    bool display_ready;
    bool test_tft;
    bool test_flash;
    bool test_eeprom;
    bool test_flipper;
    float ai_learning_rate;
    float ai_actor_learning_rate;
    float ai_critic_learning_rate;
    float ai_discount_factor;
    float ai_reward_decay;
    float ai_entropy_beta;
    float ai_policy_temperature;
    loki_tool_config_t *tools;
    size_t tool_count;
    dragon_ai_state_t dragon;
} web_ui_snapshot_t;

typedef hal_status_t (*web_ui_save_settings_fn)(const char *form_data,
                                                char *message,
                                                size_t message_size,
                                                void *context);
typedef hal_status_t (*web_ui_run_tool_fn)(size_t tool_index,
                                           char *output,
                                           size_t output_size,
                                           void *context);

typedef struct {
    uint16_t port;
    bool running;
    web_ui_snapshot_t snapshot;
#if PLATFORM == PLATFORM_LINUX
    int listen_fd;
    char bind_address[WEB_UI_TEXT_MAX];
    void *thread_handle;
    void *mutex_handle;
    web_ui_save_settings_fn save_settings;
    web_ui_run_tool_fn run_tool;
    void *handler_context;
#endif
} web_ui_t;

hal_status_t web_ui_init(web_ui_t *web_ui, const char *bind_address, uint16_t port);
hal_status_t web_ui_start(web_ui_t *web_ui);
hal_status_t web_ui_update_snapshot(web_ui_t *web_ui, const web_ui_snapshot_t *snapshot);
void web_ui_set_handlers(web_ui_t *web_ui,
                         web_ui_save_settings_fn save_settings,
                         web_ui_run_tool_fn run_tool,
                         void *context);
hal_status_t web_ui_stop(web_ui_t *web_ui);

#endif /* WEB_UI_H */
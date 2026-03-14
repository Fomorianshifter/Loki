#ifndef TOOL_RUNNER_H
#define TOOL_RUNNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dragon_ai.h"
#include "loki_config.h"
#include "types.h"

#define LOKI_GPS_SOURCE_MAX 64

typedef struct {
    bool has_fix;
    float latitude;
    float longitude;
    float altitude_m;
    char source[LOKI_GPS_SOURCE_MAX];
} loki_gps_state_t;

typedef struct {
    const loki_config_t *config;
    const dragon_ai_state_t *dragon;
    const loki_gps_state_t *gps;
    const char *nearby_networks;
    const char *request_remote_address;
    uint32_t nearby_network_count;
    uint32_t heartbeat_ticks;
    uint32_t network_phase_ms;
    bool request_is_localhost;
} loki_tool_context_t;

hal_status_t loki_tool_run(const loki_tool_config_t *tool,
                           const loki_tool_context_t *context,
                           char *output,
                           size_t output_size);

#endif /* TOOL_RUNNER_H */
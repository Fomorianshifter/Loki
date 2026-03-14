#ifndef TOOL_RUNNER_H
#define TOOL_RUNNER_H

#include <stddef.h>
#include <stdint.h>

#include "dragon_ai.h"
#include "loki_config.h"
#include "types.h"

typedef struct {
    const loki_config_t *config;
    const dragon_ai_state_t *dragon;
    uint32_t heartbeat_ticks;
    uint32_t network_phase_ms;
} loki_tool_context_t;

hal_status_t loki_tool_run(const loki_tool_config_t *tool,
                           const loki_tool_context_t *context,
                           char *output,
                           size_t output_size);

#endif /* TOOL_RUNNER_H */
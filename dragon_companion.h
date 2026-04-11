/**
 * @file dragon_companion.h
 * @brief Enhanced dragon AI with active learning, pattern recognition, and companion behaviors
 */

#ifndef DRAGON_COMPANION_H
#define DRAGON_COMPANION_H

#include <stdint.h>
#include <stdbool.h>
#include "dragon_ai.h"

#define DRAGON_MEMORY_SIZE 32
#define DRAGON_PATTERN_SIZE 8
#define DRAGON_INSIGHT_TEXT_MAX 128

typedef struct {
    char event[DRAGON_EVENT_TEXT_MAX];
    uint32_t timestamp;
    float reward_delta;
    uint32_t frequency;
} dragon_memory_t;

typedef struct {
    char pattern_name[48];
    float confidence;
    uint32_t occurrences;
    float total_reward;
    char suggestion[DRAGON_INSIGHT_TEXT_MAX];
} dragon_pattern_t;

typedef struct {
    char insight[DRAGON_INSIGHT_TEXT_MAX];
    uint32_t timestamp;
    bool active;
} dragon_insight_t;

typedef struct {
    dragon_ai_t *dragon;
    dragon_memory_t memories[DRAGON_MEMORY_SIZE];
    uint32_t memory_count;
    dragon_pattern_t patterns[DRAGON_PATTERN_SIZE];
    uint32_t pattern_count;
    dragon_insight_t insights[DRAGON_PATTERN_SIZE];
    uint32_t insight_count;
    uint32_t learning_cycles;
    uint32_t last_proactive_message_time;
    float cumulative_entropy;
    float cumulative_reward;
    bool is_learning_actively;
} dragon_companion_t;

/**
 * Initialize companion system
 */
void dragon_companion_init(dragon_companion_t *companion, dragon_ai_t *dragon);

/**
 * Update companion with new observations
 */
void dragon_companion_update(dragon_companion_t *companion, 
                            uint32_t elapsed_ms,
                            uint32_t networks_nearby,
                            uint32_t networks_known,
                            float discovery_entropy);

/**
 * Apply learning from tool execution
 */
void dragon_companion_learn_from_tool(dragon_companion_t *companion,
                                     const char *tool_name,
                                     const char *tool_output,
                                     float reward);

/**
 * Get companion's current proactive message/suggestion
 */
const char *dragon_companion_get_insight(dragon_companion_t *companion);

/**
 * Get companion's learning status
 */
float dragon_companion_get_confidence(const dragon_companion_t *companion);

/**
 * Get active pattern recognition
 */
const char *dragon_companion_get_active_pattern(const dragon_companion_t *companion);

#endif /* DRAGON_COMPANION_H */

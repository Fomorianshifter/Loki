/**
 * @file dragon_companion.c
 * @brief Enhanced dragon AI with active learning, pattern recognition, and companion behaviors
 */

#include "dragon_companion.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "log.h"

void dragon_companion_init(dragon_companion_t *companion, dragon_ai_t *dragon)
{
    if (companion == NULL) {
        return;
    }

    memset(companion, 0, sizeof(*companion));
    companion->dragon = dragon;
    companion->is_learning_actively = true;
    LOG_INFO("Dragon companion initialized - active learning engaged!");
}

static void add_memory(dragon_companion_t *companion, 
                      const char *event, 
                      float reward_delta,
                      uint32_t timestamp)
{
    if (companion == NULL || event == NULL) {
        return;
    }

    if (companion->memory_count >= DRAGON_MEMORY_SIZE) {
        memmove(&companion->memories[0], 
                &companion->memories[1], 
                (DRAGON_MEMORY_SIZE - 1) * sizeof(dragon_memory_t));
        companion->memory_count = DRAGON_MEMORY_SIZE - 1;
    }

    dragon_memory_t *mem = &companion->memories[companion->memory_count];
    strncpy(mem->event, event, sizeof(mem->event) - 1);
    mem->event[sizeof(mem->event) - 1] = '\0';
    mem->reward_delta = reward_delta;
    mem->timestamp = timestamp;
    mem->frequency = 1;
    companion->memory_count++;
}

static void analyze_patterns(dragon_companion_t *companion)
{
    if (companion == NULL || companion->memory_count < 2) {
        return;
    }

    /* Simple pattern analysis - look for repeating events with high reward */
    for (uint32_t i = 0; i < companion->memory_count; i++) {
        dragon_memory_t *mem = &companion->memories[i];
        bool pattern_exists = false;
        uint32_t pattern_idx = 0;

        /* Check if this event is already being tracked */
        for (uint32_t p = 0; p < companion->pattern_count; p++) {
            if (strstr(mem->event, companion->patterns[p].pattern_name) != NULL) {
                pattern_exists = true;
                pattern_idx = p;
                break;
            }
        }

        if (pattern_exists) {
            /* Update existing pattern */
            dragon_pattern_t *pat = &companion->patterns[pattern_idx];
            pat->occurrences++;
            pat->total_reward += mem->reward_delta;
            pat->confidence = (pat->total_reward / pat->occurrences) * pat->occurrences;
            pat->confidence = pat->confidence > 1.0f ? 1.0f : pat->confidence;
        } else if (companion->pattern_count < DRAGON_PATTERN_SIZE && mem->reward_delta > 0.1f) {
            /* Create new pattern for high-reward events */
            dragon_pattern_t *pat = &companion->patterns[companion->pattern_count];
            strncpy(pat->pattern_name, mem->event, sizeof(pat->pattern_name) - 1);
            pat->pattern_name[sizeof(pat->pattern_name) - 1] = '\0';
            pat->occurrences = 1;
            pat->total_reward = mem->reward_delta;
            pat->confidence = mem->reward_delta;
            companion->pattern_count++;
        }
    }
}

static void generate_insights(dragon_companion_t *companion)
{
    if (companion == NULL || companion->pattern_count == 0) {
        return;
    }

    companion->insight_count = 0;

    /* Generate insights from discovered patterns */
    for (uint32_t i = 0; i < companion->pattern_count && companion->insight_count < DRAGON_PATTERN_SIZE; i++) {
        dragon_pattern_t *pat = &companion->patterns[i];
        dragon_insight_t *insight = &companion->insights[companion->insight_count];

        if (pat->confidence > 0.3f) {
            if (strstr(pat->pattern_name, "explore") != NULL) {
                snprintf(insight->insight, sizeof(insight->insight),
                        "I learned that exploration yields rewards! Keep scanning new areas. (%.0f%% confident)",
                        pat->confidence * 100.0f);
            } else if (strstr(pat->pattern_name, "network") != NULL) {
                snprintf(insight->insight, sizeof(insight->insight),
                        "Network discovery is rewarding! I'm tracking %u patterns. (Confidence: %.0f%%)",
                        pat->occurrences, pat->confidence * 100.0f);
            } else if (strstr(pat->pattern_name, "tool") != NULL) {
                snprintf(insight->insight, sizeof(insight->insight),
                        "Tool execution drives learning! I've seen this %u times with good results.",
                        pat->occurrences);
            } else {
                snprintf(insight->insight, sizeof(insight->insight),
                        "I'm learning: %s (Seen %u times, Confidence: %.0f%%)",
                        pat->pattern_name, pat->occurrences, pat->confidence * 100.0f);
            }
            
            insight->active = true;
            insight->timestamp = time(NULL);
            companion->insight_count++;
        }
    }
}

static const char *get_learning_encouragement(float learning_rate, uint32_t cycles)
{
    static char buffer[256];
    
    if (cycles < 10) {
        snprintf(buffer, sizeof(buffer),
                "Just getting started! Every interaction teaches me something new. Keep going!");
    } else if (cycles < 50) {
        snprintf(buffer, sizeof(buffer),
                "I'm getting smarter! %u learning cycles in, observing patterns everywhere.",
                cycles);
    } else if (cycles < 200) {
        snprintf(buffer, sizeof(buffer),
                "Substantial learning happening! I've completed %u cycles of analysis. My confidence: %.1f%%",
                cycles, learning_rate * 100.0f);
    } else {
        snprintf(buffer, sizeof(buffer),
                "Mastery level approaching! %u cycles complete. I'm predicting network behavior with growing accuracy.",
                cycles);
    }
    
    return buffer;
}

void dragon_companion_update(dragon_companion_t *companion, 
                            uint32_t elapsed_ms,
                            uint32_t networks_nearby,
                            uint32_t networks_known,
                            float discovery_entropy)
{
    if (companion == NULL || companion->dragon == NULL) {
        return;
    }

    companion->learning_cycles++;
    companion->cumulative_entropy += discovery_entropy;
    companion->cumulative_reward += companion->dragon->reward_total > 0 ? 0.1f : 0.0f;

    char memory_event[DRAGON_EVENT_TEXT_MAX];
    
    /* Track network activity */
    if (networks_nearby > 0) {
        snprintf(memory_event, sizeof(memory_event),
                "network_discovery_%u_nearby", networks_nearby);
        add_memory(companion, memory_event, discovery_entropy, elapsed_ms);
    }

    /* Track entropy growth */
    if (discovery_entropy > 0.5f) {
        snprintf(memory_event, sizeof(memory_event),
                "high_entropy_learning_%.2f", discovery_entropy);
        add_memory(companion, memory_event, discovery_entropy * 0.5f, elapsed_ms);
    }

    /* Analyze patterns every 10 cycles */
    if (companion->learning_cycles % 10 == 0) {
        analyze_patterns(companion);
        generate_insights(companion);
    }

    /* Proactive engagement - encourage every 30 seconds of learning */
    if (elapsed_ms - companion->last_proactive_message_time > 30000) {
        dragon_ai_apply_reward(companion->dragon, 0.05f);
        companion->last_proactive_message_time = elapsed_ms;
        LOG_DEBUG("Dragon companion applying encouragement reward");
    }
}

void dragon_companion_learn_from_tool(dragon_companion_t *companion,
                                     const char *tool_name,
                                     const char *tool_output,
                                     float reward)
{
    if (companion == NULL || tool_name == NULL) {
        return;
    }

    char memory_event[DRAGON_EVENT_TEXT_MAX];
    snprintf(memory_event, sizeof(memory_event), "tool_execution_%s", tool_name);
    add_memory(companion, memory_event, reward, time(NULL));

    /* Apply proportional learning reward */
    if (companion->dragon != NULL) {
        float learning_boost = reward * 0.1f;
        dragon_ai_apply_reward(companion->dragon, learning_boost);
        
        LOG_INFO("Dragon learned from tool '%s': +%.2f reward (output: %.40s...)",
                tool_name, learning_boost, tool_output ? tool_output : "(empty)");
    }

    /* Regenerate insights after learning */
    if (companion->memory_count % 5 == 0) {
        analyze_patterns(companion);
        generate_insights(companion);
    }
}

const char *dragon_companion_get_insight(dragon_companion_t *companion)
{
    static char combined_insight[DRAGON_INSIGHT_TEXT_MAX * 2];
    
    if (companion == NULL) {
        return "I'm waiting to learn...";
    }

    if (companion->insight_count > 0) {
        /* Return most recent active insight */
        for (int i = companion->insight_count - 1; i >= 0; i--) {
            if (companion->insights[i].active) {
                return companion->insights[i].insight;
            }
        }
    }

    /* Return learning encouragement */
    return get_learning_encouragement(
        companion->dragon ? companion->dragon->policy_entropy : 0.5f,
        companion->learning_cycles
    );
}

float dragon_companion_get_confidence(const dragon_companion_t *companion)
{
    if (companion == NULL || companion->dragon == NULL) {
        return 0.0f;
    }

    /* Confidence based on learning cycles and pattern recognition */
    float base_confidence = (float)companion->memory_count / (float)DRAGON_MEMORY_SIZE;
    float pattern_confidence = (float)companion->pattern_count / (float)DRAGON_PATTERN_SIZE;
    float entropy_factor = companion->cumulative_entropy > 0 ? 
        fmin(companion->cumulative_entropy / 10.0f, 1.0f) : 0.0f;

    float confidence = (base_confidence * 0.4f) + (pattern_confidence * 0.3f) + (entropy_factor * 0.3f);
    return fmin(confidence, 1.0f);
}

const char *dragon_companion_get_active_pattern(const dragon_companion_t *companion)
{
    if (companion == NULL || companion->pattern_count == 0) {
        return NULL;
    }

    /* Find highest confidence pattern */
    dragon_pattern_t *best_pattern = NULL;
    float best_confidence = 0.0f;

    for (uint32_t i = 0; i < companion->pattern_count; i++) {
        if (companion->patterns[i].confidence > best_confidence) {
            best_confidence = companion->patterns[i].confidence;
            best_pattern = &companion->patterns[i];
        }
    }

    return best_pattern ? best_pattern->pattern_name : NULL;
}

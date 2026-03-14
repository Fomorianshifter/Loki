#ifndef DRAGON_AI_H
#define DRAGON_AI_H

/**
 * @file dragon_ai.h
 * @brief Lightweight actor-critic dragon behavior model.
 */

#include <stdint.h>

#include "types.h"

#define DRAGON_ACTION_COUNT   4
#define DRAGON_FEATURE_COUNT  4
#define DRAGON_EGG_STAGE_MAX  3U
#define DRAGON_HATCHLING_STAGE_MAX  8U
#define DRAGON_HISTORY_COUNT  6U
#define DRAGON_EVENT_TEXT_MAX  56U
#define DRAGON_EMOTION_TEXT_MAX  24U
#define DRAGON_FOCUS_TEXT_MAX  64U

typedef struct {
    char name[64];
    char profile[64];
    char temperament[64];
    float learning_rate;
    float actor_learning_rate;
    float critic_learning_rate;
    float discount_factor;
    float reward_decay;
    float entropy_beta;
    float policy_temperature;
    float base_energy;
    float base_curiosity;
    float base_bond;
    float base_hunger;
    float hunger_rate;
    float energy_decay;
    float curiosity_rate;
    float mood_hungry_threshold;
    float mood_restful_energy_threshold;
    float mood_curious_threshold;
    float mood_excited_threshold;
    float rest_bias;
    float observe_bias;
    float explore_bias;
    float play_bias;
    float rest_reward;
    float observe_reward;
    float explore_reward;
    float play_reward;
    float rest_energy_gain;
    float rest_hunger_gain;
    float observe_bond_gain;
    float explore_curiosity_cost;
    float explore_energy_cost;
    float explore_xp_gain;
    float play_bond_gain;
    float play_energy_cost;
    float play_hunger_gain;
    float play_xp_gain;
    float hunger_penalty_threshold;
    float hunger_penalty;
    uint32_t growth_interval_ms;
    float growth_xp_step;
    uint32_t egg_stage_max;
    uint32_t hatchling_stage_max;
} dragon_ai_profile_t;

typedef enum {
    DRAGON_MOOD_RESTFUL = 0,
    DRAGON_MOOD_ALERT,
    DRAGON_MOOD_CURIOUS,
    DRAGON_MOOD_HUNGRY,
    DRAGON_MOOD_EXCITED,
} dragon_mood_t;

typedef enum {
    DRAGON_ACTION_REST = 0,
    DRAGON_ACTION_OBSERVE,
    DRAGON_ACTION_EXPLORE,
    DRAGON_ACTION_PLAY,
} dragon_action_t;

typedef enum {
    DRAGON_LIFE_STAGE_EGG = 0,
    DRAGON_LIFE_STAGE_HATCHLING,
    DRAGON_LIFE_STAGE_ADULT,
} dragon_life_stage_t;

typedef struct {
    char name[64];
    uint32_t age_ticks;
    uint32_t growth_stage;
    float hunger;
    float energy;
    float curiosity;
    float bond;
    float xp;
    float last_reward;
    float last_advantage;
    float policy_confidence;
    float policy_entropy;
    float value_estimate;
    float reward_total;
    float reward_average;
    float usefulness;
    float trust;
    float vigilance;
    float comfort;
    uint32_t reward_events;
    uint32_t feed_count;
    uint32_t successful_scans;
    uint32_t successful_tools;
    uint32_t interactions;
    uint32_t recent_event_count;
    char recent_events[DRAGON_HISTORY_COUNT][DRAGON_EVENT_TEXT_MAX];
    char emotion[DRAGON_EMOTION_TEXT_MAX];
    char focus[DRAGON_FOCUS_TEXT_MAX];
    char speech[96];
    dragon_mood_t mood;
    dragon_action_t action;
} dragon_ai_state_t;

typedef struct {
    dragon_ai_state_t state;
    float learning_rate;
    float actor_learning_rate;
    float critic_learning_rate;
    float discount_factor;
    float reward_decay;
    float entropy_beta;
    float policy_temperature;
    float hunger_rate;
    float energy_decay;
    float curiosity_rate;
    float mood_hungry_threshold;
    float mood_restful_energy_threshold;
    float mood_curious_threshold;
    float mood_excited_threshold;
    float rest_reward;
    float observe_reward;
    float explore_reward;
    float play_reward;
    float rest_energy_gain;
    float rest_hunger_gain;
    float observe_bond_gain;
    float explore_curiosity_cost;
    float explore_energy_cost;
    float explore_xp_gain;
    float play_bond_gain;
    float play_energy_cost;
    float play_hunger_gain;
    float play_xp_gain;
    float hunger_penalty_threshold;
    float hunger_penalty;
    float growth_xp_step;
    float actor_weights[DRAGON_ACTION_COUNT][DRAGON_FEATURE_COUNT];
    float critic_weights[DRAGON_FEATURE_COUNT];
    float last_policy[DRAGON_ACTION_COUNT];
    float pending_reward;
    float last_value;
    uint32_t tick_accumulator_ms;
    uint32_t last_growth_ms;
    uint32_t growth_interval_ms;
    dragon_action_t last_logged_action;
} dragon_ai_t;

void dragon_ai_init(dragon_ai_t *dragon, const dragon_ai_profile_t *profile);
void dragon_ai_tick(dragon_ai_t *dragon, uint32_t tick_ms);
void dragon_ai_apply_reward(dragon_ai_t *dragon, float reward);
void dragon_ai_interact(dragon_ai_t *dragon, const char *interaction);
hal_status_t dragon_ai_save_state(const dragon_ai_t *dragon, const char *path);
hal_status_t dragon_ai_load_state(dragon_ai_t *dragon, const char *path);
const dragon_ai_state_t *dragon_ai_get_state(const dragon_ai_t *dragon);
const char *dragon_ai_action_name(dragon_action_t action);
const char *dragon_ai_mood_name(dragon_mood_t mood);
dragon_life_stage_t dragon_ai_life_stage(uint32_t growth_stage);
const char *dragon_ai_life_stage_name(uint32_t growth_stage);

#endif /* DRAGON_AI_H */

/**
 * @file dragon_ai.c
 * @brief Actor-critic dragon behavior model for Loki.
 */

#include "dragon_ai.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"

static uint32_t g_dragon_egg_stage_max = DRAGON_EGG_STAGE_MAX;
static uint32_t g_dragon_hatchling_stage_max = DRAGON_HATCHLING_STAGE_MAX;

static void dragon_copy_text(char *dest, size_t dest_size, const char *src)
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

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void build_features(const dragon_ai_t *dragon, float features[DRAGON_FEATURE_COUNT])
{
    features[0] = 1.0f - dragon->state.hunger;
    features[1] = dragon->state.energy;
    features[2] = dragon->state.curiosity;
    features[3] = dragon->state.bond;
}

static float critic_value(const dragon_ai_t *dragon, const float features[DRAGON_FEATURE_COUNT])
{
    float value = 0.0f;
    size_t index;

    for (index = 0; index < DRAGON_FEATURE_COUNT; index++) {
        value += dragon->critic_weights[index] * features[index];
    }

    return value;
}

static void build_policy(dragon_ai_t *dragon,
                         const float features[DRAGON_FEATURE_COUNT],
                         float policy[DRAGON_ACTION_COUNT])
{
    float logits[DRAGON_ACTION_COUNT];
    float max_logit = -1000000.0f;
    float total = 0.0f;
    size_t action_index;
    size_t feature_index;

    for (action_index = 0; action_index < DRAGON_ACTION_COUNT; action_index++) {
        logits[action_index] = 0.0f;
        for (feature_index = 0; feature_index < DRAGON_FEATURE_COUNT; feature_index++) {
            logits[action_index] += dragon->actor_weights[action_index][feature_index] * features[feature_index];
        }

        logits[action_index] /= dragon->policy_temperature;
        if (logits[action_index] > max_logit) {
            max_logit = logits[action_index];
        }
    }

    for (action_index = 0; action_index < DRAGON_ACTION_COUNT; action_index++) {
        policy[action_index] = expf(logits[action_index] - max_logit);
        total += policy[action_index];
    }

    if (total <= 0.0f) {
        for (action_index = 0; action_index < DRAGON_ACTION_COUNT; action_index++) {
            policy[action_index] = 1.0f / (float)DRAGON_ACTION_COUNT;
        }
        return;
    }

    for (action_index = 0; action_index < DRAGON_ACTION_COUNT; action_index++) {
        policy[action_index] /= total;
    }
}

static dragon_action_t sample_action(const float policy[DRAGON_ACTION_COUNT], float *confidence)
{
    float draw = (float)rand() / (float)RAND_MAX;
    float cumulative = 0.0f;
    size_t action_index;

    for (action_index = 0; action_index < DRAGON_ACTION_COUNT; action_index++) {
        cumulative += policy[action_index];
        if (draw <= cumulative || action_index == (DRAGON_ACTION_COUNT - 1U)) {
            if (confidence != NULL) {
                *confidence = policy[action_index];
            }
            return (dragon_action_t)action_index;
        }
    }

    if (confidence != NULL) {
        *confidence = policy[DRAGON_ACTION_REST];
    }
    return DRAGON_ACTION_REST;
}

static void update_speech(dragon_ai_t *dragon)
{
    if (dragon->state.hunger > 0.82f) {
        snprintf(dragon->state.speech,
                 sizeof(dragon->state.speech),
                 "I NEED A BREAK. HUNGER IS HIGH.");
    } else if (dragon->state.action == DRAGON_ACTION_EXPLORE) {
        snprintf(dragon->state.speech,
                 sizeof(dragon->state.speech),
                 "I AM SCANNING THE AREA AND LEARNING.");
    } else if (dragon->state.action == DRAGON_ACTION_PLAY) {
        snprintf(dragon->state.speech,
                 sizeof(dragon->state.speech),
                 "I FEEL BOLD. LET'S CHASE A NEW PATTERN.");
    } else if (dragon->state.action == DRAGON_ACTION_REST) {
        snprintf(dragon->state.speech,
                 sizeof(dragon->state.speech),
                 "I AM RECHARGING SO I CAN HATCH STRONGER.");
    } else {
        snprintf(dragon->state.speech,
                 sizeof(dragon->state.speech),
                 "MOOD %s. ACTION %s.",
                 dragon_ai_mood_name(dragon->state.mood),
                 dragon_ai_action_name(dragon->state.action));
    }
}

static void update_mood(dragon_ai_t *dragon)
{
    if (dragon->state.hunger > dragon->mood_hungry_threshold) {
        dragon->state.mood = DRAGON_MOOD_HUNGRY;
    } else if (dragon->state.energy < dragon->mood_restful_energy_threshold) {
        dragon->state.mood = DRAGON_MOOD_RESTFUL;
    } else if (dragon->state.curiosity > dragon->mood_curious_threshold) {
        dragon->state.mood = DRAGON_MOOD_CURIOUS;
    } else if (dragon->state.bond > dragon->mood_excited_threshold) {
        dragon->state.mood = DRAGON_MOOD_EXCITED;
    } else {
        dragon->state.mood = DRAGON_MOOD_ALERT;
    }
}

void dragon_ai_init(dragon_ai_t *dragon, const dragon_ai_profile_t *profile)
{
    size_t action_index;
    size_t feature_index;

    if (dragon == NULL) {
        return;
    }

    memset(dragon, 0, sizeof(*dragon));
    if (profile != NULL && profile->name[0] != '\0') {
        dragon_copy_text(dragon->state.name, sizeof(dragon->state.name), profile->name);
    } else {
        dragon_copy_text(dragon->state.name, sizeof(dragon->state.name), "Loki");
    }

    dragon->state.energy = (profile != NULL) ? profile->base_energy : 0.76f;
    dragon->state.curiosity = (profile != NULL) ? profile->base_curiosity : 0.64f;
    dragon->state.bond = (profile != NULL) ? profile->base_bond : 0.40f;
    dragon->state.hunger = (profile != NULL) ? profile->base_hunger : 0.22f;
    dragon->state.action = DRAGON_ACTION_OBSERVE;
    dragon->state.mood = DRAGON_MOOD_ALERT;
    dragon->state.value_estimate = 0.0f;
    dragon->state.policy_entropy = 0.0f;
    dragon->state.reward_total = 0.0f;
    dragon->state.reward_average = 0.0f;
    dragon->state.reward_events = 0U;
    dragon_copy_text(dragon->state.speech, sizeof(dragon->state.speech), "BOOTING MY NEST SENSES.");

    dragon->learning_rate = (profile != NULL) ? profile->learning_rate : 0.06f;
    dragon->actor_learning_rate = (profile != NULL && profile->actor_learning_rate > 0.0f) ? profile->actor_learning_rate : dragon->learning_rate;
    dragon->critic_learning_rate = (profile != NULL && profile->critic_learning_rate > 0.0f) ? profile->critic_learning_rate : dragon->learning_rate;
    dragon->discount_factor = (profile != NULL) ? profile->discount_factor : 0.92f;
    dragon->reward_decay = (profile != NULL) ? profile->reward_decay : 0.97f;
    dragon->entropy_beta = (profile != NULL) ? profile->entropy_beta : 0.015f;
    dragon->policy_temperature = (profile != NULL && profile->policy_temperature > 0.05f) ? profile->policy_temperature : 0.85f;
    dragon->hunger_rate = (profile != NULL) ? profile->hunger_rate : 0.015f;
    dragon->energy_decay = (profile != NULL) ? profile->energy_decay : 0.010f;
    dragon->curiosity_rate = (profile != NULL) ? profile->curiosity_rate : 0.012f;
    dragon->mood_hungry_threshold = (profile != NULL) ? profile->mood_hungry_threshold : 0.72f;
    dragon->mood_restful_energy_threshold = (profile != NULL) ? profile->mood_restful_energy_threshold : 0.28f;
    dragon->mood_curious_threshold = (profile != NULL) ? profile->mood_curious_threshold : 0.68f;
    dragon->mood_excited_threshold = (profile != NULL) ? profile->mood_excited_threshold : 0.70f;
    dragon->rest_reward = (profile != NULL) ? profile->rest_reward : 0.05f;
    dragon->observe_reward = (profile != NULL) ? profile->observe_reward : 0.08f;
    dragon->explore_reward = (profile != NULL) ? profile->explore_reward : 0.12f;
    dragon->play_reward = (profile != NULL) ? profile->play_reward : 0.15f;
    dragon->rest_energy_gain = (profile != NULL) ? profile->rest_energy_gain : 0.050f;
    dragon->rest_hunger_gain = (profile != NULL) ? profile->rest_hunger_gain : 0.006f;
    dragon->observe_bond_gain = (profile != NULL) ? profile->observe_bond_gain : 0.015f;
    dragon->explore_curiosity_cost = (profile != NULL) ? profile->explore_curiosity_cost : 0.080f;
    dragon->explore_energy_cost = (profile != NULL) ? profile->explore_energy_cost : 0.030f;
    dragon->explore_xp_gain = (profile != NULL) ? profile->explore_xp_gain : 0.20f;
    dragon->play_bond_gain = (profile != NULL) ? profile->play_bond_gain : 0.040f;
    dragon->play_energy_cost = (profile != NULL) ? profile->play_energy_cost : 0.040f;
    dragon->play_hunger_gain = (profile != NULL) ? profile->play_hunger_gain : 0.020f;
    dragon->play_xp_gain = (profile != NULL) ? profile->play_xp_gain : 0.30f;
    dragon->hunger_penalty_threshold = (profile != NULL) ? profile->hunger_penalty_threshold : 0.80f;
    dragon->hunger_penalty = (profile != NULL) ? profile->hunger_penalty : 0.20f;
    dragon->growth_interval_ms = (profile != NULL) ? profile->growth_interval_ms : 180000U;
    dragon->growth_xp_step = (profile != NULL) ? profile->growth_xp_step : 25.0f;

    if (profile != NULL) {
        g_dragon_egg_stage_max = profile->egg_stage_max;
        g_dragon_hatchling_stage_max = profile->hatchling_stage_max;
        if (g_dragon_hatchling_stage_max < g_dragon_egg_stage_max) {
            g_dragon_hatchling_stage_max = g_dragon_egg_stage_max;
        }
    }

    srand((unsigned int)time(NULL));

    for (action_index = 0; action_index < DRAGON_ACTION_COUNT; action_index++) {
        for (feature_index = 0; feature_index < DRAGON_FEATURE_COUNT; feature_index++) {
            dragon->actor_weights[action_index][feature_index] =
                ((float)((action_index + 1) * (feature_index + 2)) / 50.0f);
        }
        dragon->last_policy[action_index] = 1.0f / (float)DRAGON_ACTION_COUNT;
    }

    dragon->actor_weights[DRAGON_ACTION_REST][1] += (profile != NULL) ? profile->rest_bias : 0.35f;
    dragon->actor_weights[DRAGON_ACTION_OBSERVE][3] += (profile != NULL) ? profile->observe_bias : 0.25f;
    dragon->actor_weights[DRAGON_ACTION_EXPLORE][2] += (profile != NULL) ? profile->explore_bias : 0.45f;
    dragon->actor_weights[DRAGON_ACTION_PLAY][0] += (profile != NULL) ? profile->play_bias : 0.20f;
}

void dragon_ai_apply_reward(dragon_ai_t *dragon, float reward)
{
    if (dragon == NULL) {
        return;
    }

    dragon->pending_reward += reward;
    dragon->state.last_reward = reward;
}

void dragon_ai_tick(dragon_ai_t *dragon, uint32_t tick_ms)
{
    float features_before[DRAGON_FEATURE_COUNT];
    float features_after[DRAGON_FEATURE_COUNT];
    float policy[DRAGON_ACTION_COUNT];
    float td_error;
    float reward;
    float value;
    float next_value;
    float entropy = 0.0f;
    float selected_probability = 0.0f;
    size_t action_index;
    size_t feature_index;

    if (dragon == NULL) {
        return;
    }

    dragon->tick_accumulator_ms += tick_ms;
    dragon->state.age_ticks++;

    reward = dragon->pending_reward;
    dragon->pending_reward *= dragon->reward_decay;

    dragon->state.hunger = clampf(dragon->state.hunger + dragon->hunger_rate, 0.0f, 1.0f);
    dragon->state.energy = clampf(dragon->state.energy - dragon->energy_decay, 0.0f, 1.0f);
    dragon->state.curiosity = clampf(dragon->state.curiosity + dragon->curiosity_rate, 0.0f, 1.0f);

    build_features(dragon, features_before);
    value = critic_value(dragon, features_before);
    build_policy(dragon, features_before, policy);
    dragon->state.action = sample_action(policy, &selected_probability);
    dragon->state.policy_confidence = selected_probability;

    for (action_index = 0; action_index < DRAGON_ACTION_COUNT; action_index++) {
        float probability = (policy[action_index] > 0.0001f) ? policy[action_index] : 0.0001f;
        dragon->last_policy[action_index] = policy[action_index];
        entropy -= probability * logf(probability);
    }

    switch (dragon->state.action) {
        case DRAGON_ACTION_REST:
            dragon->state.energy = clampf(dragon->state.energy + dragon->rest_energy_gain, 0.0f, 1.0f);
            dragon->state.hunger = clampf(dragon->state.hunger + dragon->rest_hunger_gain, 0.0f, 1.0f);
            reward += dragon->rest_reward;
            break;
        case DRAGON_ACTION_OBSERVE:
            dragon->state.bond = clampf(dragon->state.bond + dragon->observe_bond_gain, 0.0f, 1.0f);
            reward += dragon->observe_reward;
            break;
        case DRAGON_ACTION_EXPLORE:
            dragon->state.curiosity = clampf(dragon->state.curiosity - dragon->explore_curiosity_cost, 0.0f, 1.0f);
            dragon->state.energy = clampf(dragon->state.energy - dragon->explore_energy_cost, 0.0f, 1.0f);
            dragon->state.xp += dragon->explore_xp_gain;
            reward += dragon->explore_reward;
            break;
        case DRAGON_ACTION_PLAY:
            dragon->state.bond = clampf(dragon->state.bond + dragon->play_bond_gain, 0.0f, 1.0f);
            dragon->state.energy = clampf(dragon->state.energy - dragon->play_energy_cost, 0.0f, 1.0f);
            dragon->state.hunger = clampf(dragon->state.hunger + dragon->play_hunger_gain, 0.0f, 1.0f);
            dragon->state.xp += dragon->play_xp_gain;
            reward += dragon->play_reward;
            break;
    }

    if (dragon->state.hunger > dragon->hunger_penalty_threshold) {
        reward -= dragon->hunger_penalty;
    }

    build_features(dragon, features_after);
    next_value = critic_value(dragon, features_after);
    td_error = reward + (dragon->discount_factor * next_value) - value;
    dragon->state.last_advantage = td_error;
    dragon->state.policy_entropy = entropy;

    for (feature_index = 0; feature_index < DRAGON_FEATURE_COUNT; feature_index++) {
        dragon->critic_weights[feature_index] += dragon->critic_learning_rate * td_error * features_before[feature_index];
    }

    for (action_index = 0; action_index < DRAGON_ACTION_COUNT; action_index++) {
        float policy_gradient = (((dragon_action_t)action_index == dragon->state.action) ? 1.0f : 0.0f) - policy[action_index];
        for (feature_index = 0; feature_index < DRAGON_FEATURE_COUNT; feature_index++) {
            float entropy_term = dragon->entropy_beta * entropy * features_before[feature_index];
            dragon->actor_weights[action_index][feature_index] +=
                dragon->actor_learning_rate * ((td_error * policy_gradient * features_before[feature_index]) + entropy_term);
        }
    }

    dragon->last_value = next_value;
    dragon->state.value_estimate = next_value;
    dragon->state.last_reward = reward;
    dragon->state.reward_total += reward;
    dragon->state.reward_events++;
    dragon->state.reward_average = dragon->state.reward_total / (float)dragon->state.reward_events;
    update_mood(dragon);
    update_speech(dragon);

    if (dragon->state.xp >= ((float)(dragon->state.growth_stage + 1U) * dragon->growth_xp_step) &&
        dragon->tick_accumulator_ms >= (dragon->last_growth_ms + dragon->growth_interval_ms)) {
        dragon->state.growth_stage++;
        dragon->last_growth_ms = dragon->tick_accumulator_ms;
        dragon->state.bond = clampf(dragon->state.bond + 0.10f, 0.0f, 1.0f);
        dragon_copy_text(dragon->state.speech, sizeof(dragon->state.speech), "I FEEL MY SHELL SHIFTING. I AM GROWING.");
        LOG_INFO("Dragon advanced to growth stage %u", dragon->state.growth_stage);
    }
}

const dragon_ai_state_t *dragon_ai_get_state(const dragon_ai_t *dragon)
{
    if (dragon == NULL) {
        return NULL;
    }

    return &dragon->state;
}

const char *dragon_ai_action_name(dragon_action_t action)
{
    switch (action) {
        case DRAGON_ACTION_REST: return "Rest";
        case DRAGON_ACTION_OBSERVE: return "Observe";
        case DRAGON_ACTION_EXPLORE: return "Explore";
        case DRAGON_ACTION_PLAY: return "Play";
        default: return "Unknown";
    }
}

const char *dragon_ai_mood_name(dragon_mood_t mood)
{
    switch (mood) {
        case DRAGON_MOOD_RESTFUL: return "Restful";
        case DRAGON_MOOD_ALERT: return "Alert";
        case DRAGON_MOOD_CURIOUS: return "Curious";
        case DRAGON_MOOD_HUNGRY: return "Hungry";
        case DRAGON_MOOD_EXCITED: return "Excited";
        default: return "Unknown";
    }
}

dragon_life_stage_t dragon_ai_life_stage(uint32_t growth_stage)
{
    if (growth_stage <= g_dragon_egg_stage_max) {
        return DRAGON_LIFE_STAGE_EGG;
    }
    if (growth_stage <= g_dragon_hatchling_stage_max) {
        return DRAGON_LIFE_STAGE_HATCHLING;
    }
    return DRAGON_LIFE_STAGE_ADULT;
}

const char *dragon_ai_life_stage_name(uint32_t growth_stage)
{
    switch (dragon_ai_life_stage(growth_stage)) {
        case DRAGON_LIFE_STAGE_EGG: return "Egg";
        case DRAGON_LIFE_STAGE_HATCHLING: return "Hatchling";
        case DRAGON_LIFE_STAGE_ADULT: return "Adult";
        default: return "Unknown";
    }
}
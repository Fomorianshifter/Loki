#ifndef LOKI_BEHAVIOR_H
#define LOKI_BEHAVIOR_H

#include <stdint.h>

typedef enum {
    LOKI_LIFE_STAGE_EGG = 0,
    LOKI_LIFE_STAGE_HATCHLING,
    LOKI_LIFE_STAGE_YOUNG,
    LOKI_LIFE_STAGE_ADULT,
} loki_life_stage_t;

typedef enum {
    LOKI_MOOD_CALM = 0,
    LOKI_MOOD_PLAYFUL,
    LOKI_MOOD_CURIOUS,
    LOKI_MOOD_SLEEPY,
    LOKI_MOOD_HUNGRY,
    LOKI_MOOD_GRUMPY,
    LOKI_MOOD_WITHDRAWN,
} loki_mood_t;

typedef enum {
    LOKI_ANIM_IDLE = 0,
    LOKI_ANIM_EGG_WIGGLE,
    LOKI_ANIM_NIBBLE,
    LOKI_ANIM_PLAY,
    LOKI_ANIM_REST,
    LOKI_ANIM_LOOK_AROUND,
    LOKI_ANIM_WITHDRAWN,
} loki_animation_state_t;

typedef enum {
    LOKI_BG_THEME_NEST = 0,
    LOKI_BG_THEME_NURSERY,
    LOKI_BG_THEME_GROVE,
    LOKI_BG_THEME_SKY_REALM,
} loki_background_theme_t;

typedef enum {
    LOKI_BG_ANIM_SOFT_BREATH = 0,
    LOKI_BG_ANIM_EMBER_FLICKER,
    LOKI_BG_ANIM_CLOUD_DRIFT,
    LOKI_BG_ANIM_WIND_SWAY,
    LOKI_BG_ANIM_STORM_RUMBLE,
} loki_background_anim_t;

typedef enum {
    LOKI_CARE_NONE = 0,
    LOKI_CARE_FEED,
    LOKI_CARE_PLAY,
    LOKI_CARE_COMFORT,
    LOKI_CARE_NEGLECT,
} loki_care_event_t;

typedef struct {
    uint8_t trust;
    uint8_t curiosity;
    uint8_t playfulness;
    uint8_t resilience;
    uint8_t independence;
} loki_behavior_traits_t;

typedef struct {
    loki_background_theme_t theme;
    loki_background_anim_t animation;
    uint8_t frame;
    uint8_t frame_count;
    uint16_t frame_time_ms;
    uint32_t elapsed_ms;
} loki_background_state_t;

typedef struct {
    loki_life_stage_t stage;
    uint32_t growth_points;
    uint32_t age_ticks;

    uint8_t hunger;
    uint8_t energy;

    loki_mood_t mood;
    loki_animation_state_t animation;
    loki_behavior_traits_t traits;
    loki_background_state_t background;

    loki_care_event_t last_care_event;
    uint32_t care_events;
    uint32_t neglect_ticks;
    uint32_t autonomous_actions;
} loki_dragon_state_t;

void loki_init_state(loki_dragon_state_t *state);
void loki_record_care_event(loki_dragon_state_t *state, loki_care_event_t event);
void loki_behavior_update(loki_dragon_state_t *state, uint32_t delta_ms);
void loki_autonomous_tick(loki_dragon_state_t *state, uint32_t delta_ms);

const loki_background_state_t *loki_get_background_state(const loki_dragon_state_t *state);
const char *loki_stage_name(loki_life_stage_t stage);
const char *loki_mood_name(loki_mood_t mood);
const char *loki_animation_name(loki_animation_state_t animation);
const char *loki_background_theme_name(loki_background_theme_t theme);
const char *loki_background_anim_name(loki_background_anim_t animation);

#endif /* LOKI_BEHAVIOR_H */

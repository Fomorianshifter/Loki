#include "loki_behavior.h"

#include <stddef.h>

#define LOKI_MAX_STAT 100U
#define LOKI_NEGLECT_TRAIT_UPDATE_INTERVAL 30U /* seconds without care before trust shifts */
#define LOKI_WITHDRAWN_NEGLECT_THRESHOLD 90U
#define LOKI_WITHDRAWN_TRUST_THRESHOLD 35U
#define LOKI_GRUMPY_HUNGER_THRESHOLD 65U
#define LOKI_INDEPENDENCE_GROWTH_INTERVAL 15U

static uint8_t clamp_stat(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > (int)LOKI_MAX_STAT) {
        return LOKI_MAX_STAT;
    }
    return (uint8_t)value;
}

static uint16_t stage_growth_target(loki_life_stage_t stage)
{
    switch (stage) {
        case LOKI_LIFE_STAGE_EGG:
            return 120;
        case LOKI_LIFE_STAGE_HATCHLING:
            return 360;
        case LOKI_LIFE_STAGE_YOUNG:
            return 900;
        case LOKI_LIFE_STAGE_ADULT:
        default:
            return 0;
    }
}

static void select_stage_background(loki_dragon_state_t *state)
{
    switch (state->stage) {
        case LOKI_LIFE_STAGE_EGG:
            state->background.theme = LOKI_BG_THEME_NEST;
            state->background.frame_count = 4;
            break;
        case LOKI_LIFE_STAGE_HATCHLING:
            state->background.theme = LOKI_BG_THEME_NURSERY;
            state->background.frame_count = 5;
            break;
        case LOKI_LIFE_STAGE_YOUNG:
            state->background.theme = LOKI_BG_THEME_GROVE;
            state->background.frame_count = 6;
            break;
        case LOKI_LIFE_STAGE_ADULT:
        default:
            state->background.theme = LOKI_BG_THEME_SKY_REALM;
            state->background.frame_count = 6;
            break;
    }
}

static void choose_mood(loki_dragon_state_t *state)
{
    if (state->hunger >= 80) {
        state->mood = LOKI_MOOD_HUNGRY;
    } else if (state->neglect_ticks > LOKI_WITHDRAWN_NEGLECT_THRESHOLD &&
               state->traits.trust < LOKI_WITHDRAWN_TRUST_THRESHOLD) {
        state->mood = LOKI_MOOD_WITHDRAWN;
    } else if (state->energy < 25) {
        state->mood = LOKI_MOOD_SLEEPY;
    } else if (state->traits.playfulness > 65 && state->traits.trust > 55) {
        state->mood = LOKI_MOOD_PLAYFUL;
    } else if (state->traits.curiosity > 60) {
        state->mood = LOKI_MOOD_CURIOUS;
    } else if (state->hunger > LOKI_GRUMPY_HUNGER_THRESHOLD || state->neglect_ticks > 150) {
        state->mood = LOKI_MOOD_GRUMPY;
    } else {
        state->mood = LOKI_MOOD_CALM;
    }
}

static void choose_animation(loki_dragon_state_t *state)
{
    if (state->stage == LOKI_LIFE_STAGE_EGG) {
        state->animation = (state->mood == LOKI_MOOD_SLEEPY)
            ? LOKI_ANIM_IDLE
            : LOKI_ANIM_EGG_WIGGLE;
        return;
    }

    switch (state->mood) {
        case LOKI_MOOD_HUNGRY:
            state->animation = LOKI_ANIM_NIBBLE;
            break;
        case LOKI_MOOD_PLAYFUL:
            state->animation = LOKI_ANIM_PLAY;
            break;
        case LOKI_MOOD_SLEEPY:
            state->animation = LOKI_ANIM_REST;
            break;
        case LOKI_MOOD_CURIOUS:
            state->animation = LOKI_ANIM_LOOK_AROUND;
            break;
        case LOKI_MOOD_WITHDRAWN:
            state->animation = LOKI_ANIM_WITHDRAWN;
            break;
        default:
            state->animation = LOKI_ANIM_IDLE;
            break;
    }
}

static void choose_background_animation(loki_dragon_state_t *state)
{
    switch (state->mood) {
        case LOKI_MOOD_PLAYFUL:
        case LOKI_MOOD_CURIOUS:
            state->background.animation = LOKI_BG_ANIM_CLOUD_DRIFT;
            state->background.frame_time_ms = 650;
            break;
        case LOKI_MOOD_SLEEPY:
            state->background.animation = LOKI_BG_ANIM_SOFT_BREATH;
            state->background.frame_time_ms = 1100;
            break;
        case LOKI_MOOD_HUNGRY:
        case LOKI_MOOD_GRUMPY:
            state->background.animation = LOKI_BG_ANIM_EMBER_FLICKER;
            state->background.frame_time_ms = 500;
            break;
        case LOKI_MOOD_WITHDRAWN:
            state->background.animation = LOKI_BG_ANIM_STORM_RUMBLE;
            state->background.frame_time_ms = 450;
            break;
        case LOKI_MOOD_CALM:
        default:
            state->background.animation = LOKI_BG_ANIM_WIND_SWAY;
            state->background.frame_time_ms = 800;
            break;
    }
}

static void apply_growth(loki_dragon_state_t *state, uint32_t elapsed_s)
{
    uint16_t growth_gain = 0;

    if (state->hunger < 55) {
        growth_gain += (uint16_t)(2U * elapsed_s);
    }
    if (state->mood == LOKI_MOOD_PLAYFUL || state->mood == LOKI_MOOD_CURIOUS) {
        growth_gain += (uint16_t)elapsed_s;
    }
    if (state->neglect_ticks > 180) {
        growth_gain = (growth_gain > elapsed_s) ? (uint16_t)(growth_gain - elapsed_s) : 0;
    }

    state->growth_points += growth_gain;

    while (state->stage < LOKI_LIFE_STAGE_ADULT) {
        uint16_t target = stage_growth_target(state->stage);
        if (target == 0 || state->growth_points < target) {
            break;
        }

        state->stage++;
        state->traits.resilience = clamp_stat((int)state->traits.resilience + 4);
        select_stage_background(state);
    }
}

void loki_init_state(loki_dragon_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->stage = LOKI_LIFE_STAGE_EGG;
    state->growth_points = 0;
    state->age_ticks = 0;
    state->behavior_elapsed_ms = 0;
    state->hunger = 15;
    state->energy = 85;
    state->mood = LOKI_MOOD_CALM;
    state->animation = LOKI_ANIM_EGG_WIGGLE;
    state->traits.trust = 40;
    state->traits.curiosity = 45;
    state->traits.playfulness = 30;
    state->traits.resilience = 35;
    state->traits.independence = 20;
    state->last_care_event = LOKI_CARE_NONE;
    state->care_events = 0;
    state->neglect_ticks = 0;
    state->autonomous_actions = 0;

    state->background.theme = LOKI_BG_THEME_NEST;
    state->background.animation = LOKI_BG_ANIM_SOFT_BREATH;
    state->background.frame = 0;
    state->background.frame_count = 4;
    state->background.frame_time_ms = 900;
    state->background.elapsed_ms = 0;
}

void loki_record_care_event(loki_dragon_state_t *state, loki_care_event_t event)
{
    if (state == NULL) {
        return;
    }

    state->last_care_event = event;
    state->care_events++;

    switch (event) {
        case LOKI_CARE_FEED:
            state->hunger = clamp_stat((int)state->hunger - 24);
            state->energy = clamp_stat((int)state->energy + 10);
            state->traits.trust = clamp_stat((int)state->traits.trust + 3);
            state->traits.resilience = clamp_stat((int)state->traits.resilience + 1);
            state->neglect_ticks = 0;
            break;
        case LOKI_CARE_PLAY:
            state->energy = clamp_stat((int)state->energy - 8);
            state->traits.playfulness = clamp_stat((int)state->traits.playfulness + 4);
            state->traits.curiosity = clamp_stat((int)state->traits.curiosity + 2);
            state->traits.trust = clamp_stat((int)state->traits.trust + 2);
            state->neglect_ticks = 0;
            break;
        case LOKI_CARE_COMFORT:
            state->traits.trust = clamp_stat((int)state->traits.trust + 4);
            state->traits.resilience = clamp_stat((int)state->traits.resilience + 2);
            state->energy = clamp_stat((int)state->energy + 5);
            state->neglect_ticks = 0;
            break;
        case LOKI_CARE_NEGLECT:
            state->neglect_ticks += 10;
            state->traits.trust = clamp_stat((int)state->traits.trust - 2);
            state->traits.playfulness = clamp_stat((int)state->traits.playfulness - 1);
            state->traits.independence = clamp_stat((int)state->traits.independence + 1);
            break;
        case LOKI_CARE_NONE:
        default:
            break;
    }
}

void loki_behavior_update(loki_dragon_state_t *state, uint32_t delta_ms)
{
    uint32_t elapsed_s;
    uint32_t previous_age_ticks;

    if (state == NULL || delta_ms == 0) {
        return;
    }

    state->behavior_elapsed_ms += delta_ms;
    elapsed_s = state->behavior_elapsed_ms / 1000;
    if (elapsed_s == 0) {
        return;
    }
    state->behavior_elapsed_ms %= 1000;

    previous_age_ticks = state->age_ticks;
    state->age_ticks += elapsed_s;

    state->hunger = clamp_stat((int)state->hunger + (int)elapsed_s + (int)state->stage);

    if (state->hunger > 70) {
        state->energy = clamp_stat((int)state->energy - (int)elapsed_s - 1);
    } else if (state->energy < 85) {
        state->energy = clamp_stat((int)state->energy + 1);
    }

    if (state->last_care_event == LOKI_CARE_NONE) {
        state->neglect_ticks += elapsed_s;
        if (state->neglect_ticks > 0 &&
            (state->neglect_ticks % LOKI_NEGLECT_TRAIT_UPDATE_INTERVAL) == 0) {
            state->traits.trust = clamp_stat((int)state->traits.trust - 1);
            state->traits.independence = clamp_stat((int)state->traits.independence + 1);
        }
    }

    if (state->stage >= LOKI_LIFE_STAGE_YOUNG &&
        (state->age_ticks / LOKI_INDEPENDENCE_GROWTH_INTERVAL) >
            (previous_age_ticks / LOKI_INDEPENDENCE_GROWTH_INTERVAL)) {
        state->traits.independence = clamp_stat((int)state->traits.independence + 1);
    }

    choose_mood(state);
    apply_growth(state, elapsed_s);
    choose_animation(state);
    choose_background_animation(state);

    state->last_care_event = LOKI_CARE_NONE;
}

void loki_autonomous_tick(loki_dragon_state_t *state, uint32_t delta_ms)
{
    if (state == NULL || delta_ms == 0) {
        return;
    }

    loki_behavior_update(state, delta_ms);

    state->background.elapsed_ms += delta_ms;
    /* Guard against accidental state corruption from future external state loading. */
    if (state->background.frame_count == 0) {
        state->background.frame_count = 1;
    }
    while (state->background.elapsed_ms >= state->background.frame_time_ms) {
        state->background.elapsed_ms -= state->background.frame_time_ms;
        state->background.frame = (uint8_t)((state->background.frame + 1U) % state->background.frame_count);
    }

    state->autonomous_actions++;
}

const loki_background_state_t *loki_get_background_state(const loki_dragon_state_t *state)
{
    if (state == NULL) {
        return NULL;
    }

    return &state->background;
}

const char *loki_stage_name(loki_life_stage_t stage)
{
    switch (stage) {
        case LOKI_LIFE_STAGE_EGG:
            return "egg";
        case LOKI_LIFE_STAGE_HATCHLING:
            return "hatchling";
        case LOKI_LIFE_STAGE_YOUNG:
            return "young";
        case LOKI_LIFE_STAGE_ADULT:
            return "adult";
        default:
            return "unknown";
    }
}

const char *loki_mood_name(loki_mood_t mood)
{
    switch (mood) {
        case LOKI_MOOD_CALM:
            return "calm";
        case LOKI_MOOD_PLAYFUL:
            return "playful";
        case LOKI_MOOD_CURIOUS:
            return "curious";
        case LOKI_MOOD_SLEEPY:
            return "sleepy";
        case LOKI_MOOD_HUNGRY:
            return "hungry";
        case LOKI_MOOD_GRUMPY:
            return "grumpy";
        case LOKI_MOOD_WITHDRAWN:
            return "withdrawn";
        default:
            return "unknown";
    }
}

const char *loki_animation_name(loki_animation_state_t animation)
{
    switch (animation) {
        case LOKI_ANIM_IDLE:
            return "idle";
        case LOKI_ANIM_EGG_WIGGLE:
            return "egg_wiggle";
        case LOKI_ANIM_NIBBLE:
            return "nibble";
        case LOKI_ANIM_PLAY:
            return "play";
        case LOKI_ANIM_REST:
            return "rest";
        case LOKI_ANIM_LOOK_AROUND:
            return "look_around";
        case LOKI_ANIM_WITHDRAWN:
            return "withdrawn";
        default:
            return "unknown";
    }
}

const char *loki_background_theme_name(loki_background_theme_t theme)
{
    switch (theme) {
        case LOKI_BG_THEME_NEST:
            return "nest";
        case LOKI_BG_THEME_NURSERY:
            return "nursery";
        case LOKI_BG_THEME_GROVE:
            return "grove";
        case LOKI_BG_THEME_SKY_REALM:
            return "sky_realm";
        default:
            return "unknown";
    }
}

const char *loki_background_anim_name(loki_background_anim_t animation)
{
    switch (animation) {
        case LOKI_BG_ANIM_SOFT_BREATH:
            return "soft_breath";
        case LOKI_BG_ANIM_EMBER_FLICKER:
            return "ember_flicker";
        case LOKI_BG_ANIM_CLOUD_DRIFT:
            return "cloud_drift";
        case LOKI_BG_ANIM_WIND_SWAY:
            return "wind_sway";
        case LOKI_BG_ANIM_STORM_RUMBLE:
            return "storm_rumble";
        default:
            return "unknown";
    }
}

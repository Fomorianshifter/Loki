/**
 * @file loki_life.c
 * @brief Loki Life-Cycle System — implementation
 *
 * Implements the full life-cycle engine described in loki_life.h:
 *
 *   Stage progression
 *     Loki begins as an egg.  Growth points (gp) are earned by feeding
 *     and interactions.  When gp reaches a threshold the stage advances
 *     and a log message announces the transition.
 *
 *   Mood
 *     Recalculated on every tick, feed, and interaction.  Priority order
 *     ensures the most pressing need wins:
 *       HUNGRY > SLEEPY > GRUMPY > HAPPY > PLAYFUL > NEUTRAL
 *
 *   Hunger / energy / happiness
 *     The tick function advances simulated time in seconds.  Stats drift
 *     at per-minute rates (LOKI_HUNGER_RATE, etc.) converted to fractional
 *     per-second amounts accumulated in a remainder counter so no time is
 *     lost between ticks.
 *
 *   Animation state
 *     loki_get_animation_state() encodes all display priority rules in one
 *     place so the renderer has a single enum to switch on.
 */

#include "loki_life.h"
#include "log.h"

#include <string.h>   /* memset */

/* ===================================================================
 * INTERNAL HELPERS
 * =================================================================== */

/** Clamp a uint8_t value to [0, 100]. */
static uint8_t clamp100(int32_t v)
{
    if (v < 0)   return 0;
    if (v > 100) return 100;
    return (uint8_t)v;
}

/* ===================================================================
 * STRING TABLES
 * =================================================================== */

const char *loki_stage_name(loki_stage_t stage)
{
    switch (stage) {
        case LOKI_STAGE_EGG:       return "Egg";
        case LOKI_STAGE_HATCHLING: return "Hatchling";
        case LOKI_STAGE_YOUNG:     return "Young";
        case LOKI_STAGE_ADULT:     return "Adult";
        default:                   return "Unknown";
    }
}

const char *loki_mood_name(loki_mood_t mood)
{
    switch (mood) {
        case LOKI_MOOD_NEUTRAL:  return "Neutral";
        case LOKI_MOOD_HAPPY:    return "Happy";
        case LOKI_MOOD_HUNGRY:   return "Hungry";
        case LOKI_MOOD_SLEEPY:   return "Sleepy";
        case LOKI_MOOD_PLAYFUL:  return "Playful";
        case LOKI_MOOD_GRUMPY:   return "Grumpy";
        default:                 return "Unknown";
    }
}

const char *loki_anim_name(loki_anim_t anim)
{
    switch (anim) {
        case LOKI_ANIM_EGG_IDLE:       return "egg_idle";
        case LOKI_ANIM_EGG_WIGGLE:     return "egg_wiggle";
        case LOKI_ANIM_HATCHLING_IDLE: return "hatchling_idle";
        case LOKI_ANIM_EATING:         return "eating";
        case LOKI_ANIM_SLEEPING:       return "sleeping";
        case LOKI_ANIM_HAPPY:          return "happy";
        case LOKI_ANIM_GRUMPY:         return "grumpy";
        case LOKI_ANIM_PLAYFUL:        return "playful";
        case LOKI_ANIM_IDLE:           return "idle";
        default:                       return "unknown";
    }
}

const char *loki_food_name(loki_food_t food)
{
    switch (food) {
        case LOKI_FOOD_BASIC:   return "Basic kibble";
        case LOKI_FOOD_TASTY:   return "Tasty meal";
        case LOKI_FOOD_SPECIAL: return "Special treat";
        default:                return "Unknown food";
    }
}


/* ===================================================================
 * INIT
 * =================================================================== */

void loki_init(loki_state_t *state)
{
    if (state == NULL) {
        LOG_ERROR("loki_init: NULL state pointer");
        return;
    }

    memset(state, 0, sizeof(loki_state_t));

    state->stage      = LOKI_STAGE_EGG;
    state->mood       = LOKI_MOOD_NEUTRAL;
    state->hunger     = 10;   /* starts slightly hungry — egg is freshly laid */
    state->happiness  = 50;
    state->energy     = 80;
    state->birth_time = time(NULL);

    LOG_INFO("╔══════════════════════════════════════╗");
    LOG_INFO("║   Loki has been born as an egg! 🥚   ║");
    LOG_INFO("╚══════════════════════════════════════╝");
    LOG_INFO("Feed and interact with Loki to help it grow.");
}


/* ===================================================================
 * TICK — time-based stat drift
 * =================================================================== */

/**
 * @brief Internal: apply per-minute rate over @p delta_seconds using
 *        integer accumulation to avoid losing fractional seconds.
 *
 * @param[in,out] remainder  Fractional-second carry from the last tick.
 * @param[in]     delta_sec  Seconds elapsed this tick.
 * @param[in]     rate_per_min  Whole points to apply per 60-second minute.
 * @return  Whole points to apply this tick.
 */
static uint32_t accumulate_rate(uint32_t *remainder,
                                 uint32_t delta_sec,
                                 uint32_t rate_per_min)
{
    /* Accumulate seconds; apply one point per 60 accumulated seconds */
    *remainder += delta_sec * rate_per_min;
    uint32_t points = *remainder / 60u;
    *remainder     %= 60u;
    return points;
}

void loki_tick(loki_state_t *state, uint32_t delta_seconds)
{
    if (state == NULL || delta_seconds == 0) {
        return;
    }

    /* These remainders carry fractional minutes between ticks.
     * Using static storage is fine here because only one Loki instance
     * runs at a time in this embedded application. */
    static uint32_t hunger_rem   = 0;
    static uint32_t energy_rem   = 0;
    static uint32_t happy_rem    = 0;

    state->uptime_seconds += delta_seconds;

    /* --- Hunger increases over time --- */
    uint32_t hunger_inc = accumulate_rate(&hunger_rem, delta_seconds,
                                          LOKI_HUNGER_RATE);
    state->hunger = clamp100((int32_t)state->hunger + (int32_t)hunger_inc);

    /* --- Energy drains over time (unless sleeping restores it) --- */
    if (state->mood == LOKI_MOOD_SLEEPY) {
        /* While Loki is sleeping, energy recovers instead of draining */
        uint32_t energy_rec = accumulate_rate(&energy_rem, delta_seconds,
                                               LOKI_ENERGY_DRAIN * 3u);
        state->energy = clamp100((int32_t)state->energy + (int32_t)energy_rec);
    } else {
        uint32_t energy_dec = accumulate_rate(&energy_rem, delta_seconds,
                                               LOKI_ENERGY_DRAIN);
        state->energy = clamp100((int32_t)state->energy - (int32_t)energy_dec);
    }

    /* --- Happiness decays when hungry or neglected --- */
    uint32_t happy_dec = accumulate_rate(&happy_rem, delta_seconds,
                                          LOKI_HAPPY_DECAY);
    /* Decay is doubled when very hungry */
    if (state->hunger > 70) {
        happy_dec *= 2u;
    }
    state->happiness = clamp100((int32_t)state->happiness - (int32_t)happy_dec);

    /* --- Eating animation timer --- */
    if (state->eating_ticks > 0) {
        state->eating_ticks--;
    }

    loki_update_mood(state);
    loki_check_progression(state);
}


/* ===================================================================
 * FEED
 * =================================================================== */

void loki_feed(loki_state_t *state, loki_food_t food)
{
    if (state == NULL) {
        return;
    }

    uint8_t hunger_reduce;
    uint8_t growth_gain;
    uint8_t happy_gain;

    switch (food) {
        case LOKI_FOOD_BASIC:
            hunger_reduce = 20;
            growth_gain   = 5;
            happy_gain    = 5;
            break;
        case LOKI_FOOD_TASTY:
            hunger_reduce = 40;
            growth_gain   = 10;
            happy_gain    = 15;
            break;
        case LOKI_FOOD_SPECIAL:
            hunger_reduce = 60;
            growth_gain   = 20;
            happy_gain    = 25;
            break;
        default:
            hunger_reduce = 20;
            growth_gain   = 5;
            happy_gain    = 5;
            break;
    }

    /* Overeating penalty: wasted food when barely hungry */
    if (state->hunger < 10) {
        LOG_DEBUG("Loki is not very hungry — food effect halved");
        growth_gain   /= 2u;
        happy_gain    /= 2u;
        hunger_reduce /= 2u;
    }

    state->hunger    = clamp100((int32_t)state->hunger    - (int32_t)hunger_reduce);
    state->happiness = clamp100((int32_t)state->happiness + (int32_t)happy_gain);
    state->growth_points += growth_gain;
    state->feed_count++;

    /* Trigger the eating animation for a few ticks */
    state->eating_ticks = LOKI_EATING_ANIM_TICKS;

    loki_update_mood(state);
    loki_check_progression(state);

    LOG_INFO("Loki ate %s — hunger: %u, happiness: %u, growth_pts: %u",
             loki_food_name(food), state->hunger,
             state->happiness, state->growth_points);
}


/* ===================================================================
 * INTERACT
 * =================================================================== */

void loki_interact(loki_state_t *state)
{
    if (state == NULL) {
        return;
    }

    /* Playing boosts happiness and grants a small growth bonus */
    uint8_t happy_gain  = 10;
    uint8_t growth_gain = 2;

    /* Grumpy Loki doesn't enjoy interactions as much */
    if (state->mood == LOKI_MOOD_GRUMPY) {
        happy_gain  = 5;
        growth_gain = 1;
    }

    /* Extra energy cost for playing */
    uint8_t energy_cost = 5;

    state->happiness = clamp100((int32_t)state->happiness + (int32_t)happy_gain);
    state->energy    = clamp100((int32_t)state->energy    - (int32_t)energy_cost);
    state->growth_points += growth_gain;
    state->interact_count++;

    loki_update_mood(state);
    loki_check_progression(state);

    LOG_INFO("Interacted with Loki — happiness: %u, energy: %u, growth_pts: %u",
             state->happiness, state->energy, state->growth_points);
}


/* ===================================================================
 * MOOD UPDATE
 * =================================================================== */

void loki_update_mood(loki_state_t *state)
{
    if (state == NULL) {
        return;
    }

    loki_mood_t prev_mood = state->mood;

    /*
     * Priority ladder — the first condition that fires wins.
     * This order reflects urgency: hunger and sleep are survival needs;
     * happiness/playfulness are secondary.
     */
    if (state->hunger >= 70) {
        state->mood = LOKI_MOOD_HUNGRY;
    } else if (state->energy <= 20) {
        state->mood = LOKI_MOOD_SLEEPY;
    } else if (state->happiness <= 30 || state->hunger >= 50) {
        state->mood = LOKI_MOOD_GRUMPY;
    } else if (state->happiness >= 70) {
        state->mood = LOKI_MOOD_HAPPY;
    } else if (state->happiness >= 50 && state->energy >= 60) {
        state->mood = LOKI_MOOD_PLAYFUL;
    } else {
        state->mood = LOKI_MOOD_NEUTRAL;
    }

    if (state->mood != prev_mood) {
        LOG_INFO("Loki's mood changed: %s → %s",
                 loki_mood_name(prev_mood), loki_mood_name(state->mood));
    }
}


/* ===================================================================
 * STAGE PROGRESSION
 * =================================================================== */

void loki_check_progression(loki_state_t *state)
{
    if (state == NULL) {
        return;
    }

    loki_stage_t next_stage;
    uint32_t     threshold;

    switch (state->stage) {
        case LOKI_STAGE_EGG:
            next_stage = LOKI_STAGE_HATCHLING;
            threshold  = LOKI_THRESHOLD_HATCHLING;
            break;
        case LOKI_STAGE_HATCHLING:
            next_stage = LOKI_STAGE_YOUNG;
            threshold  = LOKI_THRESHOLD_YOUNG;
            break;
        case LOKI_STAGE_YOUNG:
            next_stage = LOKI_STAGE_ADULT;
            threshold  = LOKI_THRESHOLD_ADULT;
            break;
        default:
            /* Already adult — nothing to do */
            return;
    }

    if (state->growth_points >= threshold) {
        LOG_INFO("╔══════════════════════════════════════════════╗");
        LOG_INFO("║  Loki evolved!  %s → %s  🎉",
                 loki_stage_name(state->stage),
                 loki_stage_name(next_stage));
        LOG_INFO("╚══════════════════════════════════════════════╝");
        state->stage = next_stage;

        /* Reset eating animation on stage change to avoid stale state */
        state->eating_ticks = 0;
    }
}


/* ===================================================================
 * ANIMATION STATE SELECTION
 * =================================================================== */

loki_anim_t loki_get_animation_state(const loki_state_t *state)
{
    if (state == NULL) {
        return LOKI_ANIM_EGG_IDLE;
    }

    /* Eating animation takes priority on any stage */
    if (state->eating_ticks > 0) {
        return LOKI_ANIM_EATING;
    }

    /* Egg-specific animations */
    if (state->stage == LOKI_STAGE_EGG) {
        /* Wiggle when close to hatching */
        if (state->growth_points + LOKI_EGG_WIGGLE_MARGIN >= LOKI_THRESHOLD_HATCHLING) {
            return LOKI_ANIM_EGG_WIGGLE;
        }
        return LOKI_ANIM_EGG_IDLE;
    }

    /* Post-hatch: map mood → animation */
    switch (state->mood) {
        case LOKI_MOOD_SLEEPY:
            return LOKI_ANIM_SLEEPING;

        case LOKI_MOOD_HAPPY:
            return LOKI_ANIM_HAPPY;

        case LOKI_MOOD_GRUMPY:
        case LOKI_MOOD_HUNGRY:
            return LOKI_ANIM_GRUMPY;

        case LOKI_MOOD_PLAYFUL:
            return LOKI_ANIM_PLAYFUL;

        default:
            break;
    }

    /* Hatchling gets its own idle */
    if (state->stage == LOKI_STAGE_HATCHLING) {
        return LOKI_ANIM_HATCHLING_IDLE;
    }

    return LOKI_ANIM_IDLE;
}


/* ===================================================================
 * STATUS PRINT
 * =================================================================== */

void loki_print_status(const loki_state_t *state)
{
    if (state == NULL) {
        return;
    }

    loki_anim_t anim = loki_get_animation_state(state);

    LOG_INFO("╔══════════════════════════════════════════════╗");
    LOG_INFO("║           Loki Status Report                 ║");
    LOG_INFO("╠══════════════════════════════════════════════╣");
    LOG_INFO("║ Stage       : %-30s ║", loki_stage_name(state->stage));
    LOG_INFO("║ Mood        : %-30s ║", loki_mood_name(state->mood));
    LOG_INFO("║ Animation   : %-30s ║", loki_anim_name(anim));
    LOG_INFO("╠══════════════════════════════════════════════╣");
    LOG_INFO("║ Hunger      : %3u / 100                       ║", state->hunger);
    LOG_INFO("║ Happiness   : %3u / 100                       ║", state->happiness);
    LOG_INFO("║ Energy      : %3u / 100                       ║", state->energy);
    LOG_INFO("╠══════════════════════════════════════════════╣");
    LOG_INFO("║ Growth pts  : %-10u                      ║", state->growth_points);
    LOG_INFO("║ Feedings    : %-10u                      ║", state->feed_count);
    LOG_INFO("║ Interactions: %-10u                      ║", state->interact_count);
    LOG_INFO("║ Uptime (s)  : %-10u                      ║", state->uptime_seconds);
    LOG_INFO("╚══════════════════════════════════════════════╝");
}

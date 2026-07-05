/**
 * @file loki_life.h
 * @brief Loki Life-Cycle System — stage progression, mood, feeding, and animation state
 *
 * This module models Loki as a living creature that:
 *   - starts as an egg and grows through four life stages
 *   - accumulates growth points from feeding and interactions
 *   - maintains hunger, happiness, and energy stats that drive mood
 *   - exposes an animation/display state so the render layer always
 *     knows what to show without having to inspect raw stats
 *
 * Typical call sequence:
 *   loki_init(&state);               // once at startup
 *   loki_tick(&state, delta_secs);   // every main-loop tick
 *   loki_feed(&state, FOOD_BASIC);   // when user feeds Loki
 *   loki_interact(&state);           // when user taps/plays with Loki
 *   anim = loki_get_animation_state(&state);  // decide what to draw
 */

#ifndef LOKI_LIFE_H
#define LOKI_LIFE_H

#include <stdint.h>
#include <time.h>

/* ===================================================================
 * ENUMS
 * =================================================================== */

/**
 * @brief Life stages — determines size, available animations, and behaviour.
 *
 * Growth-point thresholds for advancing:
 *   EGG       →  HATCHLING :  50 gp
 *   HATCHLING →  YOUNG     : 200 gp
 *   YOUNG     →  ADULT     : 500 gp
 */
typedef enum {
    LOKI_STAGE_EGG       = 0,  /**< Starting stage — still in the shell */
    LOKI_STAGE_HATCHLING = 1,  /**< Just hatched — small and fragile     */
    LOKI_STAGE_YOUNG     = 2,  /**< Growing — more active and expressive  */
    LOKI_STAGE_ADULT     = 3,  /**< Fully grown                           */
} loki_stage_t;

/**
 * @brief Emotional / behavioural mood.
 *
 * Mood is recalculated after every tick, feed, and interaction.
 * Priority order (highest wins):
 *   HUNGRY  > SLEEPY > GRUMPY > HAPPY > PLAYFUL > NEUTRAL
 */
typedef enum {
    LOKI_MOOD_NEUTRAL  = 0,  /**< Calm, nothing special happening      */
    LOKI_MOOD_HAPPY    = 1,  /**< Well-fed, rested, and recently petted */
    LOKI_MOOD_HUNGRY   = 2,  /**< Hunger bar is critically high         */
    LOKI_MOOD_SLEEPY   = 3,  /**< Energy is depleted                    */
    LOKI_MOOD_PLAYFUL  = 4,  /**< High happiness and energy             */
    LOKI_MOOD_GRUMPY   = 5,  /**< Neglected or very hungry              */
} loki_mood_t;

/**
 * @brief Food types that can be offered to Loki.
 *
 * Higher-quality food reduces more hunger, yields more growth points,
 * and gives a larger happiness boost.
 */
typedef enum {
    LOKI_FOOD_BASIC   = 0,  /**< Plain kibble  — small effect    */
    LOKI_FOOD_TASTY   = 1,  /**< Nice meal     — medium effect   */
    LOKI_FOOD_SPECIAL = 2,  /**< Rare treat    — large effect    */
} loki_food_t;

/**
 * @brief Animation / display states.
 *
 * The render layer queries this instead of reading raw stats directly,
 * keeping display and logic cleanly separated.
 */
typedef enum {
    LOKI_ANIM_EGG_IDLE       = 0,  /**< Egg sitting still (default egg state)  */
    LOKI_ANIM_EGG_WIGGLE     = 1,  /**< Egg rocking — near hatch threshold      */
    LOKI_ANIM_HATCHLING_IDLE = 2,  /**< Hatchling standing, blinking            */
    LOKI_ANIM_EATING         = 3,  /**< Chewing animation (any stage)           */
    LOKI_ANIM_SLEEPING       = 4,  /**< Eyes closed / ZZZ (any stage)           */
    LOKI_ANIM_HAPPY          = 5,  /**< Big smile / bounce                      */
    LOKI_ANIM_GRUMPY         = 6,  /**< Frowning / sulking                      */
    LOKI_ANIM_PLAYFUL        = 7,  /**< Running / jumping                       */
    LOKI_ANIM_IDLE           = 8,  /**< Default idle for hatchling/young/adult  */
} loki_anim_t;


/* ===================================================================
 * STATE MODEL
 * =================================================================== */

/**
 * @brief Complete runtime state of Loki.
 *
 * All stats use a 0–100 scale unless noted otherwise.
 * growth_points is unbounded and drives stage transitions.
 */
typedef struct {
    /* --- Life stage --- */
    loki_stage_t stage;       /**< Current life stage                          */
    loki_mood_t  mood;        /**< Current mood (derived, updated each tick)   */

    /* --- Vital stats (0–100) --- */
    uint8_t hunger;           /**< 0 = full, 100 = starving                    */
    uint8_t happiness;        /**< 0 = miserable, 100 = ecstatic               */
    uint8_t energy;           /**< 0 = exhausted, 100 = fully rested           */

    /* --- Progression --- */
    uint32_t growth_points;   /**< Total accumulated growth points (XP)        */
    uint32_t feed_count;      /**< Lifetime number of feedings                 */
    uint32_t interact_count;  /**< Lifetime number of interactions             */

    /* --- Time tracking --- */
    time_t   birth_time;      /**< Unix timestamp of loki_init() call          */
    uint32_t uptime_seconds;  /**< Total simulated seconds since birth         */

    /* --- Internal animation hint --- */
    uint8_t  eating_ticks;    /**< Remaining ticks to display eating animation */
} loki_state_t;


/* ===================================================================
 * THRESHOLDS & RATES  (compile-time tunables)
 * =================================================================== */

/** Growth points required to advance each stage */
#define LOKI_THRESHOLD_HATCHLING  50u   /**< egg       → hatchling */
#define LOKI_THRESHOLD_YOUNG     200u   /**< hatchling → young     */
#define LOKI_THRESHOLD_ADULT     500u   /**< young     → adult     */

/** Hunger increases by this many points per simulated minute */
#define LOKI_HUNGER_RATE      1u

/** Energy drains by this many points per simulated minute */
#define LOKI_ENERGY_DRAIN     1u

/** Happiness decays by this many points per simulated minute */
#define LOKI_HAPPY_DECAY      1u

/** Number of ticks the eating animation is shown */
#define LOKI_EATING_ANIM_TICKS  3u

/** Egg wiggle threshold: show wiggle when this close to hatching */
#define LOKI_EGG_WIGGLE_MARGIN  10u

/** Energy recovery multiplier while Loki is sleeping (vs normal drain rate) */
#define LOKI_ENERGY_RECOVERY_MULT  3u


/* ===================================================================
 * API
 * =================================================================== */

/**
 * @brief Initialise Loki as a fresh egg.
 *
 * Must be called once before any other loki_* function.
 *
 * @param[out] state  Pointer to the state struct to initialise.
 */
void loki_init(loki_state_t *state);

/**
 * @brief Advance Loki's internal clock by @p delta_seconds seconds.
 *
 * Call this every main-loop iteration with the elapsed real or simulated
 * time.  It updates hunger, energy, happiness, and rechecks mood/stage.
 *
 * @param[in,out] state         Loki state.
 * @param[in]     delta_seconds Elapsed seconds since last tick.
 */
void loki_tick(loki_state_t *state, uint32_t delta_seconds);

/**
 * @brief Feed Loki the given food type.
 *
 * Reduces hunger, boosts happiness, and awards growth points.
 * Triggers a re-evaluation of mood and stage.
 *
 * @param[in,out] state Loki state.
 * @param[in]     food  Food type to offer.
 */
void loki_feed(loki_state_t *state, loki_food_t food);

/**
 * @brief Register a user interaction (petting, playing, tapping).
 *
 * Boosts happiness, awards a small growth-point bonus, and counts toward
 * the lifetime interaction total.
 *
 * @param[in,out] state Loki state.
 */
void loki_interact(loki_state_t *state);

/**
 * @brief Recalculate Loki's mood from current stats.
 *
 * Called automatically by loki_tick, loki_feed, and loki_interact.
 * May also be called directly if stats are modified externally.
 *
 * @param[in,out] state Loki state.
 */
void loki_update_mood(loki_state_t *state);

/**
 * @brief Check whether Loki should advance to the next life stage.
 *
 * Called automatically by loki_tick, loki_feed, and loki_interact.
 *
 * @param[in,out] state Loki state.
 */
void loki_check_progression(loki_state_t *state);

/**
 * @brief Return the animation state the renderer should display.
 *
 * Encodes priority rules so the render layer needs no knowledge of
 * raw stats or thresholds.
 *
 * @param[in] state Loki state (read-only).
 * @return    The animation state to render.
 */
loki_anim_t loki_get_animation_state(const loki_state_t *state);

/**
 * @brief Log a human-readable status summary (INFO level).
 *
 * @param[in] state Loki state (read-only).
 */
void loki_print_status(const loki_state_t *state);

/**
 * @brief Return a C-string name for a life stage (for logging).
 * @param[in] stage Life stage.
 * @return    Pointer to a static string.
 */
const char *loki_stage_name(loki_stage_t stage);

/**
 * @brief Return a C-string name for a mood (for logging).
 * @param[in] mood Mood value.
 * @return    Pointer to a static string.
 */
const char *loki_mood_name(loki_mood_t mood);

/**
 * @brief Return a C-string name for an animation state (for logging).
 * @param[in] anim Animation state.
 * @return    Pointer to a static string.
 */
const char *loki_anim_name(loki_anim_t anim);

/**
 * @brief Return a C-string name for a food type (for logging).
 * @param[in] food Food type.
 * @return    Pointer to a static string.
 */
const char *loki_food_name(loki_food_t food);

#endif /* LOKI_LIFE_H */

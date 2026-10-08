#ifndef DRAGON_ANIM_H
#define DRAGON_ANIM_H

/**
 * @file dragon_anim.h
 * @brief Procedural dragon animation engine for the Loki TFT display
 *
 * A state-of-the-art, fully procedural animation - no stored bitmaps or
 * sprite sheets. Every frame is synthesized per-pixel in a small RGB565
 * frame tile and streamed to the ILI9488 over SPI:
 *
 *   - Domain-warped aurora plasma sky (layered sinusoidal flow fields)
 *   - A dragon built from a chain of capsule SDF segments following a
 *     Lissajous flight path, with analytic soft shading and rim light
 *   - Additive fire glow and drifting ember particles
 *   - Fixed-point friendly math (float used only where cheap: ARMv7 FPU)
 *
 * The engine renders into a caller-provided tile buffer and pushes tiles
 * through tft_write_pixels(), so peak RAM use stays at a few KiB - well
 * within the budget of the target boards.
 */

#include "types.h"

/* ===== TUNABLES ===== */

/** Frame tile height in scanlines. Width is always TFT_WIDTH (480).
 *  40 lines x 480 px x 2 B = 38,400 B per tile buffer, x2 for
 *  double-buffered DMA-style upload. */
#define DRAGON_TILE_LINES      40

/** Total frames in one full animation loop (~6 s at 30 fps target) */
#define DRAGON_ANIM_FRAMES     180

/** Number of ember particles */
#define DRAGON_EMBER_COUNT     24

/* ===== PUBLIC API ===== */

/**
 * Initialize the dragon animation engine.
 * Requires the TFT driver to be initialized (tft_init).
 * @return HAL_OK on success
 */
hal_status_t dragon_anim_init(void);

/**
 * Render and display the full dragon animation sequence.
 * Blocks for the duration of the sequence (~6 s), then returns so the
 * caller can continue. Safe to call again for a replay.
 * @return HAL_OK on success, HAL_NOT_READY if TFT is unavailable
 */
hal_status_t dragon_anim_play(void);

/**
 * Render a single frame at time t (0.0 - 1.0 normalized over the loop).
 * Exposed for integration into custom loops.
 * @param[in] t Normalized animation time, wraps at 1.0
 * @return HAL_OK on success
 */
hal_status_t dragon_anim_render_frame(float t);

/**
 * Release animation resources.
 * @return HAL_OK on success
 */
hal_status_t dragon_anim_deinit(void);

#endif /* DRAGON_ANIM_H */

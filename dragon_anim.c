/**
 * @file dragon_anim.c
 * @brief Procedural dragon animation engine for the ILI9488 TFT
 *
 * Rendering pipeline (per frame, per tile):
 *   1. Sky: domain-warped aurora plasma. A base vertical gradient is
 *      perturbed by two layers of rotating sine flow fields; the warped
 *      coordinate drives a teal->violet->ember palette.
 *   2. Dragon: the body is a chain of N capsules whose centers follow a
 *      Lissajous flight path with per-segment phase lag, producing an
 *      organic serpentine undulation. Each pixel evaluates the minimum
 *      capsule signed distance; shading is analytic (normal from the SDF
 *      gradient, key light + cool rim light, belly glow).
 *   3. Fire: an additive radial glow at the head, pulsing with a
 *      smooth periodic envelope; ember particles advect upward with
 *      curl-ish wobble and fade out.
 *   4. Composite: sky + dragon (alpha from SDF coverage) + additive glow,
 *      written straight into an RGB565 tile and pushed with
 *      tft_write_pixels().
 *
 * Everything is computed - there are no stored sprites, so the whole
 * effect costs only code size plus one tile of RAM.
 */

#include "dragon_anim.h"
#include "tft_driver.h"
#include "config.h"
#include "memory.h"
#include "log.h"

#include <math.h>
#include <string.h>

/* ===== CONFIG ===== */

#define DRAGON_SEGMENTS      9       /* capsules along the body */
#define DRAGON_MAX_RADIUS    26.0f   /* head capsule radius (px) */
#define DRAGON_MIN_RADIUS    7.0f    /* tail capsule radius (px) */
#define FIRE_PULSE_HZ        0.6f    /* breathing fire pulse */

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ===== STATE ===== */

typedef struct {
    uint8_t  initialized;
    color_t *tile;                   /* RGB565 tile buffer */
} dragon_ctx_t;

static dragon_ctx_t dragon_ctx = {0};

/* ===== COLOR HELPERS ===== */

/** Clamp a float to [0, 1] */
static inline float clampf01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

/** Convert floating RGB (0..1) to RGB565 */
static inline color_t rgbf_to_565(float r, float g, float b)
{
    return RGB565((uint8_t)(clampf01(r) * 255.0f),
                  (uint8_t)(clampf01(g) * 255.0f),
                  (uint8_t)(clampf01(b) * 255.0f));
}

/** Smoothstep */
static inline float smoothf(float a, float b, float x)
{
    float t = (x - a) / (b - a);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

/* ===== SKY ===== */

/**
 * Domain-warped aurora plasma. Two nested warp fields give the sky a
 * flowing, nebular motion that never visibly repeats within the loop.
 */
static void sky_pixel(float x, float y, float t, float *r, float *g, float *b)
{
    const float w = (float)TFT_WIDTH;
    const float h = (float)TFT_HEIGHT;
    float u = x / w;
    float v = y / h;
    float T = t * 2.0f * (float)M_PI;

    /* Warp layer 1: slow horizontal drift */
    float w1 = sinf(u * 6.0f + T * 0.7f) * 0.05f
             + sinf(v * 4.0f - T * 0.4f) * 0.04f;
    /* Warp layer 2: finer detail riding on layer 1 */
    float w2 = sinf((u + w1) * 14.0f - T * 1.1f)
             * sinf((v - w1) * 9.0f + T * 0.9f) * 0.05f;

    float uw = u + w1 + w2;
    float vw = v + w1 * 0.6f - w2;

    /* Aurora band: sinuous bright ribbon across the upper sky */
    float band = sinf(uw * 3.0f + sinf(vw * 5.0f + T * 0.5f) + T * 0.3f);
    band = 0.5f + 0.5f * band;
    band = powf(band, 3.0f) * (1.0f - vw) * (1.0f - vw);

    /* Base night gradient: deep indigo at top, warm horizon ember */
    float rr = 0.03f + 0.20f * (1.0f - vw) * 0.35f;
    float gg = 0.02f + 0.05f * (1.0f - vw);
    float bb = 0.10f + 0.22f * (1.0f - vw);

    /* Aurora: teal -> violet mix */
    rr += band * 0.25f;
    gg += band * 0.75f;
    bb += band * 0.55f;

    /* Sparse twinkling stars: hash of quantized position, gated by time */
    float sx = floorf(x / 6.0f);
    float sy = floorf(y / 6.0f);
    float hash = sinf(sx * 127.1f + sy * 311.7f) * 43758.5453f;
    hash -= floorf(hash);
    float twinkle = 0.5f + 0.5f * sinf(T * 2.0f + hash * 40.0f);
    float star = (hash > 0.985f) ? smoothf(0.5f, 1.0f, twinkle) * (1.0f - vw) : 0.0f;
    rr += star * 0.9f;
    gg += star * 0.9f;
    bb += star * 1.0f;

    *r = rr; *g = gg; *b = bb;
}

/* ===== DRAGON BODY ===== */

/** Body capsule segment count and sizing */
#define HEAD_INDEX        0
#define HEAD_RADIUS       20.0f
#define SNOUT_LEN         26.0f
#define HORN_LEN          20.0f

/**
 * Flight path: Lissajous with a slow vertical bob. Segment i trails the
 * head by a phase offset so the body ripples like a ribbon.
 */
static void dragon_segment_center(int i, float t, float *cx, float *cy)
{
    float T = t * 2.0f * (float)M_PI;
    float lag = (float)i * 0.45f;
    float w = (float)TFT_WIDTH;
    float h = (float)TFT_HEIGHT;

    /* Head sweeps a wide figure-8-ish path across the screen */
    float bx = 0.5f * w + 0.36f * w * sinf(T * 1.0f + 0.3f);
    float by = 0.52f * h + 0.20f * h * sinf(T * 2.0f + 1.1f);

    /* Trailing segments: lag + lateral ripple (the "undulation") */
    float ripple = sinf(T * 2.0f - lag * 2.2f) * (6.0f + 14.0f * (float)i / DRAGON_SEGMENTS);
    *cx = bx - (float)i * 30.0f + sinf(T * 1.0f - lag) * 18.0f;
    *cy = by + ripple + (float)i * 3.0f * cosf(T * 0.7f - lag);
}

/** Capsule radius profile: big head, tapering tail */
static float dragon_segment_radius(int i)
{
    float s = (float)i / (float)(DRAGON_SEGMENTS - 1);
    /* Smooth taper with a slight chest bulge near the head */
    float body = DRAGON_MAX_RADIUS * (1.0f - s) + DRAGON_MIN_RADIUS * s;
    float bulge = 6.0f * expf(-powf((s - 0.18f) * 4.0f, 2.0f));
    return body + bulge;
}

/**
 * Signed distance from point (px,py) to capsule (ax,ay)-(bx,by) radius r.
 */
static float sd_capsule(float px, float py, float ax, float ay,
                        float bx, float by, float r)
{
    float pax = px - ax, pay = py - ay;
    float bax = bx - ax, bay = by - ay;
    float hh = bax * bax + bay * bay;
    float t = (hh > 0.0001f) ? (pax * bax + pay * bay) / hh : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float dx = pax - bax * t;
    float dy = pay - bay * t;
    return sqrtf(dx * dx + dy * dy) - r;
}

/**
 * Head direction unit vector (from neck toward snout), recomputed per
 * evaluation so all head features stay attached to the moving head.
 */
static void dragon_head_frame(float t, float *hx, float *hy, float *dx, float *dy)
{
    float nx, ny;
    dragon_segment_center(0, t, hx, hy);
    dragon_segment_center(1, t, &nx, &ny);
    float fx = *hx - nx, fy = *hy - ny;
    float fl = sqrtf(fx * fx + fy * fy);
    if (fl < 0.001f) { fx = 1.0f; fy = 0.0f; fl = 1.0f; }
    *dx = fx / fl;
    *dy = fy / fl;
}

/** Signed distance to a 2D isoceles triangle (spike) pointing along (dx,dy) */
static float sd_spike(float px, float py, float bx, float by,
                      float dx, float dy, float len, float wid)
{
    /* Rotate into spike-local space: q.x along dir, q.y perpendicular */
    float qx = (px - bx) * dx + (py - by) * dy;
    float qy = -(px - bx) * dy + (py - by) * dx;
    if (qx < 0.0f) qx = 0.0f;
    if (qx > len) qx = len;
    /* Distance to the slanted edge line: y = wid*(1 - x/len) */
    float edge = wid * (1.0f - qx / len);
    float d = fabsf(qy) - edge * 0.5f;
    float dbase = (qx <= 0.0f) ? fabsf(qy) : 0.0f;
    return (d > dbase) ? d : dbase;
}

/**
 * Evaluate the dragon SDF at a pixel. Returns min distance and (through
 * out params) the closest feature id for shading:
 *   >= 0  body segment index
 *   -1    eye        -2  snout       -3  horn        -4  dorsal spike
 */
static float dragon_sdf(float px, float py, float t, int *out_seg)
{
    float best = 1e9f;
    int best_seg = HEAD_INDEX;

    /* ---- Body: capsule chain ---- */
    float cx0 = 0.0f, cy0 = 0.0f;
    for (int i = 0; i < DRAGON_SEGMENTS; i++) {
        float cx1, cy1;
        dragon_segment_center(i, t, &cx1, &cy1);
        if (i > 0) {
            float r = dragon_segment_radius(i);
            float d = sd_capsule(px, py, cx0, cy0, cx1, cy1, r);
            if (d < best) { best = d; best_seg = i; }
        }
        cx0 = cx1; cy0 = cy1;
    }

    /* ---- Head features ---- */
    float hx, hy, fdx, fdy;
    dragon_head_frame(t, &hx, &hy, &fdx, &fdy);
    /* Perpendicular (left side of the facing direction) */
    float px_ = -fdy, py_ = fdx;

    /* Snout: tapered capsule ahead of the head */
    float s0x = hx + fdx * (HEAD_RADIUS * 0.4f);
    float s0y = hy + fdy * (HEAD_RADIUS * 0.4f);
    float s1x = hx + fdx * (HEAD_RADIUS + SNOUT_LEN);
    float s1y = hy + fdy * (HEAD_RADIUS + SNOUT_LEN);
    float d = sd_capsule(px, py, s0x, s0y, s1x, s1y, HEAD_RADIUS * 0.45f);
    if (d < best) { best = d; best_seg = -2; }

    /* Jaw: slightly offset, thinner, under the snout */
    float j0x = hx + fdx * (HEAD_RADIUS * 0.5f) + px_ * 3.0f;
    float j0y = hy + fdy * (HEAD_RADIUS * 0.5f) + py_ * 3.0f;
    float j1x = hx + fdx * (HEAD_RADIUS + SNOUT_LEN * 0.8f) + px_ * 5.0f;
    float j1y = hy + fdy * (HEAD_RADIUS + SNOUT_LEN * 0.8f) + py_ * 5.0f;
    d = sd_capsule(px, py, j0x, j0y, j1x, j1y, HEAD_RADIUS * 0.30f);
    if (d < best) { best = d; best_seg = -2; }

    /* Horns: two spikes sweeping back from the crown */
    for (int h = 0; h < 2; h++) {
        float side = (h == 0) ? 1.0f : -1.0f;
        float bx = hx - fdx * 6.0f + px_ * side * 8.0f;
        float by = hy - fdy * 6.0f + py_ * side * 8.0f;
        /* Horn direction: back and outward */
        float hdx = -fdx * 0.8f + px_ * side * 0.6f;
        float hdy = -fdy * 0.8f + py_ * side * 0.6f;
        float hl = sqrtf(hdx * hdx + hdy * hdy);
        hdx /= hl; hdy /= hl;
        d = sd_spike(px, py, bx, by, hdx, hdy, HORN_LEN, 7.0f);
        if (d < best) { best = d; best_seg = -3; }
    }

    /* Eyes: glowing dots flanking the snout base */
    for (int e = 0; e < 2; e++) {
        float side = (e == 0) ? 1.0f : -1.0f;
        float ex = hx + fdx * (HEAD_RADIUS * 0.55f) + px_ * side * 9.0f;
        float ey = hy + fdy * (HEAD_RADIUS * 0.55f) + py_ * side * 9.0f;
        float ddx = px - ex, ddy = py - ey;
        float de = sqrtf(ddx * ddx + ddy * ddy) - 2.6f;
        if (de < best) { best = de; best_seg = -1; }
    }

    /* Dorsal spikes: a fin ridge running from neck to tail */
    for (int i = 1; i < DRAGON_SEGMENTS; i++) {
        float cx, cy;
        dragon_segment_center(i, t, &cx, &cy);
        /* Spike base sits on the body's top side (screen-up) */
        float r = dragon_segment_radius(i);
        float wob = sinf(t * 2.0f * (float)M_PI * 2.0f - (float)i * 1.9f) * 0.25f;
        float sdx = wob, sdy = -1.0f;       /* mostly up, slight sway */
        float sl = sqrtf(sdx * sdx + sdy * sdy);
        sdx /= sl; sdy /= sl;
        float spike_len = r * (1.1f - 0.5f * (float)i / DRAGON_SEGMENTS);
        d = sd_spike(px, py, cx + sdx * r * 0.5f, cy + sdy * r * 0.5f,
                     sdx, sdy, spike_len, 5.5f);
        if (d < best) { best = d; best_seg = -4; }
    }

    if (out_seg) *out_seg = best_seg;
    return best;
}

/* ===== EMBERS ===== */

typedef struct {
    float seed_x, seed_y, seed_w, seed_r;
} ember_seed_t;

static ember_seed_t embers[DRAGON_EMBER_COUNT];
static uint8_t embers_seeded = 0;

static void ember_seed_init(void)
{
    /* Deterministic pseudo-random seeds (stable across runs) */
    uint32_t s = 0xC0FFEE42u;
    for (int i = 0; i < DRAGON_EMBER_COUNT; i++) {
        s = s * 1664525u + 1013904223u;
        embers[i].seed_x = (float)(s >> 8 & 0xFFFF) / 65535.0f;
        s = s * 1664525u + 1013904223u;
        embers[i].seed_y = (float)(s >> 8 & 0xFFFF) / 65535.0f;
        s = s * 1664525u + 1013904223u;
        embers[i].seed_w = (float)(s >> 8 & 0xFFFF) / 65535.0f;
        s = s * 1664525u + 1013904223u;
        embers[i].seed_r = (float)(s >> 8 & 0xFFFF) / 65535.0f;
    }
    embers_seeded = 1;
}

/**
 * Additive ember contribution at pixel. Embers rise from the lower third,
 * wobble sideways, and fade over their life. Loops forever.
 */
static void ember_contrib(float px, float py, float t,
                          float *r, float *g, float *b)
{
    float w = (float)TFT_WIDTH;
    float h = (float)TFT_HEIGHT;
    float acc_r = 0.0f, acc_g = 0.0f, acc_b = 0.0f;

    for (int i = 0; i < DRAGON_EMBER_COUNT; i++) {
        ember_seed_t *e = &embers[i];
        float life = fmodf(t * (0.5f + e->seed_w * 0.8f) + e->seed_y, 1.0f);
        float ex = e->seed_x * w + sinf(life * 9.0f + e->seed_w * 20.0f) * 14.0f;
        float ey = h * (1.0f - life) * 0.9f + h * 0.05f;
        float rad = 1.5f + e->seed_r * 2.0f;

        float dx = px - ex, dy = py - ey;
        float d2 = dx * dx + dy * dy;
        float fade = (1.0f - life) * life * 4.0f;       /* in-out fade */
        float inten = expf(-d2 / (2.0f * rad * rad)) * fade * 0.9f;

        /* Ember color: hot orange core fading to red */
        acc_r += inten * 1.0f;
        acc_g += inten * 0.42f;
        acc_b += inten * 0.10f;
    }

    *r = acc_r; *g = acc_g; *b = acc_b;
}

/* ===== FIRE GLOW (at the head) ===== */

static void fire_contrib(float px, float py, float t,
                         float *r, float *g, float *b)
{
    float hx, hy;
    dragon_segment_center(0, t, &hx, &hy);

    /* Pulsing envelope: slow breath with fast flicker on top */
    float T = t * 2.0f * (float)M_PI;
    float pulse = 0.65f + 0.35f * sinf(T * FIRE_PULSE_HZ * 6.0f);
    pulse *= 0.9f + 0.1f * sinf(T * 13.0f + sinf(T * 7.0f));

    float dx = px - hx, dy = py - hy;
    float d2 = dx * dx + dy * dy;
    float sigma = 34.0f * pulse;
    float glow = expf(-d2 / (2.0f * sigma * sigma));

    *r = glow * 0.95f;
    *g = glow * 0.45f;
    *b = glow * 0.12f;
}

/* ===== SHADING ===== */

/**
 * Shade a dragon body pixel. Normal is estimated from the SDF gradient;
 * key light upper-left, cool rim from behind, warm belly from the fire.
 */
static void dragon_shade(float px, float py, float t, float dist, int seg,
                         float *r, float *g, float *b)
{
    if (seg == -1) {
        /* Eye: hot white-gold */
        *r = 1.0f; *g = 0.85f; *b = 0.35f;
        return;
    }
    if (seg == -3 || seg == -4) {
        /* Horns / dorsal spikes: dark obsidian with a hot rim */
        *r = 0.16f; *g = 0.10f; *b = 0.14f;
        return;
    }
    if (seg == -2) seg = 0;     /* snout/jaw shades like the head */

    /* Numeric gradient of the SDF for a pseudo-normal */
    float eps = 1.5f;
    float dx = dragon_sdf(px + eps, py, t, NULL) - dragon_sdf(px - eps, py, t, NULL);
    float dy = dragon_sdf(px, py + eps, t, NULL) - dragon_sdf(px, py - eps, t, NULL);
    float nl = sqrtf(dx * dx + dy * dy);
    if (nl < 1e-5f) { dx = 0.0f; dy = -1.0f; nl = 1.0f; }
    float nx = dx / nl, ny = dy / nl;

    /* Key light */
    float lx = -0.577f, ly = -0.577f;               /* normalized-ish up-left */
    float diff = nx * lx + ny * ly;
    if (diff < 0.0f) diff = 0.0f;

    /* Rim light: strong when the normal points away from the key */
    float rim = powf(1.0f - fabsf(diff), 3.0f) * 0.9f;

    /* Belly warmth from the fire glow (light from below) */
    float belly = (ny > 0.0f) ? ny * 0.35f : 0.0f;

    /* Base scale color shifts along the body: emerald -> deep teal */
    float s = (float)seg / (float)(DRAGON_SEGMENTS - 1);
    float base_r = 0.05f + 0.10f * s;
    float base_g = 0.38f - 0.14f * s;
    float base_b = 0.24f + 0.06f * s;

    /* Scale texture: subtle stripes along the spine */
    float stripe = 0.5f + 0.5f * sinf((px + py) * 0.35f + (float)seg * 1.7f);
    stripe = 0.85f + 0.15f * stripe;

    float rr = base_r * (0.35f + 0.75f * diff) * stripe + rim * 0.25f + belly * 0.50f;
    float gg = base_g * (0.35f + 0.75f * diff) * stripe + rim * 0.55f + belly * 0.22f;
    float bb = base_b * (0.35f + 0.75f * diff) * stripe + rim * 0.60f + belly * 0.06f;

    /* Soft edge: darken to silhouette right at the boundary */
    float edge = smoothf(-1.5f, -3.5f, dist);
    rr *= edge; gg *= edge; bb *= edge;

    *r = rr; *g = gg; *b = bb;
}

/* ===== FRAME RENDER ===== */

hal_status_t dragon_anim_render_frame(float t)
{
    if (!dragon_ctx.initialized || dragon_ctx.tile == NULL) {
        return HAL_NOT_READY;
    }

    if (t < 0.0f) t = 0.0f;
    t = fmodf(t, 1.0f);

    const uint16_t W = TFT_WIDTH;
    const uint16_t H = TFT_HEIGHT;

    for (uint16_t ty = 0; ty < H; ty += DRAGON_TILE_LINES) {
        uint16_t lines = (uint16_t)(H - ty);
        if (lines > DRAGON_TILE_LINES) lines = DRAGON_TILE_LINES;

        color_t *px_out = dragon_ctx.tile;

        for (uint16_t y = 0; y < lines; y++) {
            float gy = (float)(ty + y) + 0.5f;
            for (uint16_t x = 0; x < W; x++) {
                float gx = (float)x + 0.5f;

                /* 1. Sky */
                float sr, sg, sb;
                sky_pixel(gx, gy, t, &sr, &sg, &sb);

                /* 2. Dragon (SDF alpha compositing) */
                int seg = 0;
                float d = dragon_sdf(gx, gy, t, &seg);
                if (d < 1.5f) {
                    float dr, dg, db;
                    dragon_shade(gx, gy, t, d, seg, &dr, &dg, &db);
                    float alpha = 1.0f - smoothf(-0.8f, 0.8f, d);
                    sr = sr * (1.0f - alpha) + dr * alpha;
                    sg = sg * (1.0f - alpha) + dg * alpha;
                    sb = sb * (1.0f - alpha) + db * alpha;
                }

                /* 3. Additive fire glow + embers */
                float fr, fg, fb;
                fire_contrib(gx, gy, t, &fr, &fg, &fb);
                sr += fr; sg += fg; sb += fb;
                ember_contrib(gx, gy, t, &fr, &fg, &fb);
                sr += fr; sg += fg; sb += fb;

                *px_out++ = rgbf_to_565(sr, sg, sb);
            }
        }

        hal_status_t st = tft_write_pixels(0, ty, W, lines, dragon_ctx.tile);
        if (st != HAL_OK) {
            return st;
        }
    }

    return HAL_OK;
}

/* ===== PUBLIC API ===== */

hal_status_t dragon_anim_init(void)
{
    if (dragon_ctx.initialized) {
        return HAL_OK;
    }

    if (!embers_seeded) {
        ember_seed_init();
    }

    dragon_ctx.tile = (color_t *)malloc_safe(
        (size_t)TFT_WIDTH * DRAGON_TILE_LINES * sizeof(color_t));
    if (dragon_ctx.tile == NULL) {
        LOG_ERROR("Dragon animation: tile buffer allocation failed");
        return HAL_ERROR;
    }

    dragon_ctx.initialized = 1;
    LOG_INFO("Dragon animation engine initialized (%dx%d tiles)",
             TFT_WIDTH, DRAGON_TILE_LINES);
    return HAL_OK;
}

hal_status_t dragon_anim_play(void)
{
    if (!dragon_ctx.initialized) {
        if (dragon_anim_init() != HAL_OK) {
            return HAL_NOT_READY;
        }
    }

    LOG_INFO("Playing dragon animation (%d frames)...", DRAGON_ANIM_FRAMES);

    for (int f = 0; f < DRAGON_ANIM_FRAMES; f++) {
        float t = (float)f / (float)DRAGON_ANIM_FRAMES;
        hal_status_t st = dragon_anim_render_frame(t);
        if (st != HAL_OK) {
            LOG_ERROR("Dragon animation frame %d failed (%d)", f, st);
            return st;
        }
    }

    LOG_INFO("Dragon animation complete");
    return HAL_OK;
}

hal_status_t dragon_anim_deinit(void)
{
    if (!dragon_ctx.initialized) {
        return HAL_OK;
    }

    if (dragon_ctx.tile != NULL) {
        free_safe((void **)&dragon_ctx.tile);
    }

    dragon_ctx.initialized = 0;
    return HAL_OK;
}

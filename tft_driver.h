#ifndef TFT_DRIVER_H
#define TFT_DRIVER_H

/**
 * TFT Display Driver for 3.5" ILI9488 480×320 v1.0
 * Communicates via SPI0
 */

#include "../../includes/types.h"
#include "../../config/board_config.h"

/* ===== TFT DISPLAY CONTROL ===== */

/**
 * Initialize TFT display
 * @return HAL_OK on success
 */
hal_status_t tft_init(void);

/**
 * Write pixel data to TFT (full-color)
 * @param[in] x X coordinate
 * @param[in] y Y coordinate
 * @param[in] width Width of data
 * @param[in] height Height of data
 * @param[in] data Pointer to RGB565 color data
 * @return HAL_OK on success
 */
hal_status_t tft_write_pixels(uint16_t x, uint16_t y, uint16_t width, uint16_t height, 
                              const color_t *data);

/**
 * Fill rectangle with single color
 * @param[in] x X coordinate
 * @param[in] y Y coordinate
 * @param[in] width Width
 * @param[in] height Height
 * @param[in] color RGB565 color
 * @return HAL_OK on success
 */
hal_status_t tft_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, color_t color);

/**
 * Display clear (fill entire screen with black)
 * @return HAL_OK on success
 */
hal_status_t tft_clear(void);

/**
 * Set backlight brightness
 * @param[in] brightness 0-100 (percent)
 * @return HAL_OK on success
 */
hal_status_t tft_set_brightness(uint8_t brightness);

/**
 * Set display rotation
 * @param[in] rotation 0=Normal, 1=90°, 2=180°, 3=270°
 * @return HAL_OK on success
 */
hal_status_t tft_set_rotation(uint8_t rotation);

/**
 * Deinitialize TFT
 * @return HAL_OK on success
 */
hal_status_t tft_deinit(void);

#endif /* TFT_DRIVER_H */

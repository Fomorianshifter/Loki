#ifndef GPIO_H
#define GPIO_H

/**
 * @file gpio.h
 * @brief GPIO Hardware Abstraction Layer for Orange Pi Zero 2W
 * 
 * Provides GPIO pin control interface for digital I/O operations.
 * Supports pin configuration, output control, and input reading.
 * 
 * @defgroup GPIO GPIO Pin Control
 * @{
 */

#include "../../includes/types.h"

/* ===== PUBLIC API ===== */

/**
 * @brief Initialize GPIO subsystem
 * 
 * Prepares the GPIO module for operation. Must be called once
 * before any other GPIO operations.
 * 
 * @return @ref HAL_OK on success
 * @return @ref HAL_ERROR if initialization fails
 */
hal_status_t gpio_init(void);

/**
 * @brief Configure a GPIO pin
 * 
 * Sets up a GPIO pin with specified mode (input/output),
 * pull configuration, and other parameters.
 * 
 * @param[in] config GPIO configuration structure containing:
 *   - pin: Pin number to configure
 *   - mode: GPIO_MODE_INPUT, GPIO_MODE_OUTPUT, or GPIO_MODE_ALTERNATE
 *   - pull: Pull-up, pull-down, or no pull
 * 
 * @return @ref HAL_OK if configuration successful
 * @return @ref HAL_INVALID_PARAM if config is NULL or invalid
 * @return @ref HAL_ERROR if hardware configuration fails
 * 
 * @example
 * @code
 * gpio_config_t cfg = {
 *     .pin = 18,
 *     .mode = GPIO_MODE_OUTPUT,
 *     .pull = GPIO_PULL_NONE,
 * };
 * gpio_configure(&cfg);
 * @endcode
 */
hal_status_t gpio_configure(const gpio_config_t *config);

/**
 * @brief Set GPIO pin output level
 * 
 * Drives the GPIO pin to HIGH or LOW voltage level.
 * Pin must be configured as output before calling this function.
 * 
 * @param[in] pin GPIO pin number
 * @param[in] level GPIO_LEVEL_HIGH or GPIO_LEVEL_LOW
 * 
 * @return @ref HAL_OK on success
 * @return @ref HAL_INVALID_PARAM if level is invalid
 * @return @ref HAL_ERROR if pin is not configured as output
 */
hal_status_t gpio_set(uint32_t pin, gpio_level_t level);

/**
 * @brief Read GPIO pin input level
 * 
 * Reads the current state of a GPIO pin configured as input.
 * Pin must be configured as input before calling this function.
 * 
 * @param[in] pin GPIO pin number
 * @param[out] level Pointer to store the read level (HIGH/LOW)
 * 
 * @return @ref HAL_OK on success
 * @return @ref HAL_INVALID_PARAM if level pointer is NULL
 * @return @ref HAL_ERROR if pin is not configured as input
 * 
 * @example
 * @code
 * gpio_level_t pin_state;
 * if (gpio_read(18, &pin_state) == HAL_OK) {
 *     if (pin_state == GPIO_LEVEL_HIGH) {
 *         LOG_INFO("Pin 18 is HIGH");
 *     }
 * }
 * @endcode
 */
hal_status_t gpio_read(uint32_t pin, gpio_level_t *level);

/**
 * @brief Toggle GPIO pin output level
 * 
 * Switches the output level from HIGH to LOW or vice versa.
 * Pin must be configured as output before calling this function.
 * 
 * @param[in] pin GPIO pin number
 * 
 * @return @ref HAL_OK on success
 * @return @ref HAL_ERROR if pin is not configured as output
 */
hal_status_t gpio_toggle(uint32_t pin);

/**
 * @brief Deinitialize GPIO subsystem
 * 
 * Releases GPIO resources and prepares for shutdown.
 * No GPIO operations are valid after calling this function.
 * 
 * @return @ref HAL_OK on success
 */
hal_status_t gpio_deinit(void);

/** @} */ // end of GPIO group

#endif /* GPIO_H */

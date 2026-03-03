#ifndef PWM_H
#define PWM_H

/**
 * PWM Hardware Abstraction Layer for Orange Pi Zero 2W
 * Used for TFT backlight brightness control
 */

#include "../../includes/types.h"

/* ===== PWM DEFINITIONS ===== */
typedef enum {
    PWM_CHANNEL_0 = 0,  /* TFT Backlight on GPIO Pin 7 */
    PWM_CHANNEL_COUNT = 1,
} pwm_channel_t;

/* ===== PUBLIC API ===== */

/**
 * Initialize PWM channel
 * @param[in] channel PWM channel number
 * @param[in] config PWM configuration
 * @return HAL_OK on success
 */
hal_status_t pwm_init(pwm_channel_t channel, const pwm_config_t *config);

/**
 * Set PWM duty cycle
 * @param[in] channel PWM channel number
 * @param[in] duty_cycle Duty cycle in percentage (0-100)
 * @return HAL_OK on success
 */
hal_status_t pwm_set_duty(pwm_channel_t channel, uint8_t duty_cycle);

/**
 * Set PWM frequency
 * @param[in] channel PWM channel number
 * @param[in] frequency Frequency in Hz
 * @return HAL_OK on success
 */
hal_status_t pwm_set_frequency(pwm_channel_t channel, uint32_t frequency);

/**
 * Get current PWM duty cycle
 * @param[in] channel PWM channel number
 * @return Current duty cycle (0-100), or negative value on error
 */
int pwm_get_duty(pwm_channel_t channel);

/**
 * Enable PWM output
 * @param[in] channel PWM channel number
 * @return HAL_OK on success
 */
hal_status_t pwm_enable(pwm_channel_t channel);

/**
 * Disable PWM output
 * @param[in] channel PWM channel number
 * @return HAL_OK on success
 */
hal_status_t pwm_disable(pwm_channel_t channel);

/**
 * Deinitialize PWM channel
 * @param[in] channel PWM channel number
 * @return HAL_OK on success
 */
hal_status_t pwm_deinit(pwm_channel_t channel);

#endif /* PWM_H */

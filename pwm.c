/**
 * PWM Hardware Abstraction Layer Implementation
 * Raspberry Pi - TFT Backlight Control
 */

#include "pwm.h"
#include "gpio.h"

/* ===== PWM CHANNEL CONTEXT ===== */
typedef struct {
    pwm_channel_t channel;
    uint32_t pin;
    uint32_t frequency;
    uint8_t duty_cycle;
    uint8_t enabled;
    uint8_t initialized;
} pwm_channel_context_t;

/* PWM channel context */
static pwm_channel_context_t pwm_channel_0 = {
    .channel = PWM_CHANNEL_0,
    .pin = 0,
    .frequency = 1000,
    .duty_cycle = 50,
    .enabled = 0,
    .initialized = 0,
};

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t pwm_init(pwm_channel_t channel, const pwm_config_t *config)
{
    if (config == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (channel != PWM_CHANNEL_0) {
        return HAL_INVALID_PARAM;
    }

    pwm_channel_context_t *ctx = &pwm_channel_0;

    if (ctx->initialized) {
        return HAL_OK;
    }

    /* Store configuration */
    ctx->pin = config->pin;
    ctx->frequency = config->frequency;
    ctx->duty_cycle = config->duty_cycle;

    gpio_config_t gpio_cfg = {
        .pin = ctx->pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull = GPIO_PULL_NONE,
    };

    if (gpio_configure(&gpio_cfg) != HAL_OK) {
        return HAL_ERROR;
    }
    if (gpio_set(ctx->pin, GPIO_LEVEL_LOW) != HAL_OK) {
        return HAL_ERROR;
    }

    ctx->initialized = 1;
    return HAL_OK;
}

hal_status_t pwm_set_duty(pwm_channel_t channel, uint8_t duty_cycle)
{
    if (duty_cycle > 100) {
        return HAL_INVALID_PARAM;
    }

    if (channel != PWM_CHANNEL_0) {
        return HAL_INVALID_PARAM;
    }

    pwm_channel_context_t *ctx = &pwm_channel_0;

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    ctx->duty_cycle = duty_cycle;
    if (ctx->enabled) {
        gpio_level_t level = (ctx->duty_cycle > 0) ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW;
        if (gpio_set(ctx->pin, level) != HAL_OK) {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

hal_status_t pwm_set_frequency(pwm_channel_t channel, uint32_t frequency)
{
    if (frequency == 0) {
        return HAL_INVALID_PARAM;
    }

    if (channel != PWM_CHANNEL_0) {
        return HAL_INVALID_PARAM;
    }

    pwm_channel_context_t *ctx = &pwm_channel_0;

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    /* Set frequency */
    ctx->frequency = frequency;

    /* Write to PWM period register or sysfs */
    /* Period = 1000000 / frequency (in microseconds) */

    return HAL_OK;
}

int pwm_get_duty(pwm_channel_t channel)
{
    if (channel != PWM_CHANNEL_0) {
        return -1;
    }

    pwm_channel_context_t *ctx = &pwm_channel_0;

    if (!ctx->initialized) {
        return -1;
    }

    return (int)ctx->duty_cycle;
}

hal_status_t pwm_enable(pwm_channel_t channel)
{
    if (channel != PWM_CHANNEL_0) {
        return HAL_INVALID_PARAM;
    }

    pwm_channel_context_t *ctx = &pwm_channel_0;

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    ctx->enabled = 1;
    if (gpio_set(ctx->pin, (ctx->duty_cycle > 0) ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW) != HAL_OK) {
        return HAL_ERROR;
    }
    return HAL_OK;
}

hal_status_t pwm_disable(pwm_channel_t channel)
{
    if (channel != PWM_CHANNEL_0) {
        return HAL_INVALID_PARAM;
    }

    pwm_channel_context_t *ctx = &pwm_channel_0;

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    if (gpio_set(ctx->pin, GPIO_LEVEL_LOW) != HAL_OK) {
        return HAL_ERROR;
    }
    ctx->enabled = 0;
    return HAL_OK;
}

hal_status_t pwm_deinit(pwm_channel_t channel)
{
    if (channel != PWM_CHANNEL_0) {
        return HAL_INVALID_PARAM;
    }

    pwm_channel_context_t *ctx = &pwm_channel_0;

    if (!ctx->initialized) {
        return HAL_OK;
    }

    /* Disable PWM */
    pwm_disable(channel);

    /* Deinitialize PWM */
    ctx->initialized = 0;
    return HAL_OK;
}

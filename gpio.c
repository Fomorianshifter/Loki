/**
 * @file gpio.c
 * @brief GPIO Hardware Abstraction Layer Implementation
 * Orange Pi Zero 2W
 */

#include "gpio.h"
#include "../../utils/log.h"

/* Import system resources from /sys/class/gpio (Linux sysfs) */
static const char *GPIO_SYSFS_PATH = "/sys/class/gpio";

/* ===== LOCAL FUNCTIONS ===== */

/**
 * Export GPIO pin via sysfs
 */
static hal_status_t gpio_export(uint32_t pin)
{
    LOG_DEBUG("Exporting GPIO pin %u", pin);
    /* This implementation assumes sysfs GPIO access on Linux */
    /* For actual deployment, replace with direct register access */
    return HAL_OK;
}

/**
 * Unexport GPIO pin via sysfs
 */
static hal_status_t gpio_unexport(uint32_t pin)
{
    LOG_DEBUG("Unexporting GPIO pin %u", pin);
    return HAL_OK;
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t gpio_init(void)
{
    LOG_INFO("Initializing GPIO subsystem");
    /* Initialize GPIO subsystem */
    /* On Orange Pi: usually handled by the kernel and accessible via sysfs */
    return HAL_OK;
}

hal_status_t gpio_configure(const gpio_config_t *config)
{
    if (config == NULL) {
        LOG_ERROR("GPIO configuration failed: config is NULL");
        return HAL_INVALID_PARAM;
    }

    LOG_DEBUG("Configuring GPIO pin %u (mode=%d, pull=%d)", 
             config->pin, config->mode, config->pull);

    /* Export the GPIO pin */
    hal_status_t status = gpio_export(config->pin);
    if (status != HAL_OK) {
        LOG_ERROR("Failed to export GPIO pin %u", config->pin);
        return status;
    }

    /* Set pin direction */
    switch (config->mode) {
        case GPIO_MODE_INPUT:
            LOG_DEBUG("Set GPIO pin %u as INPUT", config->pin);
            break;
        case GPIO_MODE_OUTPUT:
            LOG_DEBUG("Set GPIO pin %u as OUTPUT", config->pin);
            break;
        case GPIO_MODE_ALTERNATE:
            LOG_DEBUG("Set GPIO pin %u as ALTERNATE function", config->pin);
            break;
        default:
            LOG_ERROR("Invalid GPIO mode: %d", config->mode);
            return HAL_INVALID_PARAM;
    }

    return HAL_OK;
}

hal_status_t gpio_set(uint32_t pin, gpio_level_t level)
{
    if (level > GPIO_LEVEL_HIGH) {
        LOG_ERROR("Invalid GPIO level: %d", level);
        return HAL_INVALID_PARAM;
    }

    LOG_DEBUG("GPIO pin %u set to %s", pin, (level == GPIO_LEVEL_HIGH) ? "HIGH" : "LOW");
    /* Set pin output level */
    return HAL_OK;
}

hal_status_t gpio_read(uint32_t pin, gpio_level_t *level)
{
    if (level == NULL) {
        LOG_ERROR("GPIO read failed: level pointer is NULL");
        return HAL_INVALID_PARAM;
    }

    /* Read pin input level */
    *level = GPIO_LEVEL_LOW;
    LOG_DEBUG("GPIO pin %u read as %s", pin, (*level == GPIO_LEVEL_HIGH) ? "HIGH" : "LOW");
    return HAL_OK;
}

hal_status_t gpio_toggle(uint32_t pin)
{
    LOG_DEBUG("Toggling GPIO pin %u", pin);
    
    gpio_level_t current;
    hal_status_t status = gpio_read(pin, &current);
    if (status != HAL_OK) {
        LOG_ERROR("Failed to read GPIO pin %u for toggle", pin);
        return status;
    }

    gpio_level_t new_level = (current == GPIO_LEVEL_HIGH) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH;
    return gpio_set(pin, new_level);
}

hal_status_t gpio_deinit(void)
{
    LOG_INFO("Deinitializing GPIO subsystem");
    /* Deinitialize GPIO subsystem */
    return HAL_OK;
}

/**
 * @file gpio.c
 * @brief GPIO Hardware Abstraction Layer Implementation
 * Raspberry Pi
 */

#include "gpio.h"
#include "log.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Import system resources from /sys/class/gpio (Linux sysfs) */
static const char *GPIO_SYSFS_PATH = "/sys/class/gpio";
static uint8_t configured_pins[256] = {0};

/* ===== LOCAL FUNCTIONS ===== */

/**
 * Export GPIO pin via sysfs
 */
static hal_status_t gpio_export(uint32_t pin)
{
    char path[64];
    char value[16];
    int fd;
    ssize_t written;

    if (pin > 255) {
        LOG_ERROR("GPIO export failed: invalid pin %u", pin);
        return HAL_INVALID_PARAM;
    }

    (void)snprintf(path, sizeof(path), "%s/export", GPIO_SYSFS_PATH);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        LOG_ERROR("GPIO export failed opening %s: %s", path, strerror(errno));
        return HAL_ERROR;
    }

    (void)snprintf(value, sizeof(value), "%u", pin);
    written = write(fd, value, strlen(value));
    close(fd);

    if (written < 0 && errno != EBUSY) {
        LOG_ERROR("GPIO export failed for pin %u: %s", pin, strerror(errno));
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
 * Unexport GPIO pin via sysfs
 */
static hal_status_t gpio_unexport(uint32_t pin)
{
    char path[64];
    char value[16];
    int fd;
    ssize_t written;

    if (pin > 255) {
        return HAL_INVALID_PARAM;
    }

    (void)snprintf(path, sizeof(path), "%s/unexport", GPIO_SYSFS_PATH);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        return HAL_ERROR;
    }

    (void)snprintf(value, sizeof(value), "%u", pin);
    written = write(fd, value, strlen(value));
    close(fd);
    if (written < 0) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t gpio_init(void)
{
    LOG_INFO("Initializing GPIO subsystem");
    /* Initialize GPIO subsystem */
    /* On Raspberry Pi: usually handled by the kernel and accessible via sysfs */
    return HAL_OK;
}

hal_status_t gpio_configure(const gpio_config_t *config)
{
    char path[64];
    int fd;
    const char *direction;
    ssize_t written;

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
            direction = "in";
            break;
        case GPIO_MODE_OUTPUT:
            direction = "out";
            break;
        case GPIO_MODE_ALTERNATE:
            LOG_ERROR("GPIO alternate mode is not supported via sysfs");
            return HAL_NOT_SUPPORTED;
        default:
            LOG_ERROR("Invalid GPIO mode: %d", config->mode);
            return HAL_INVALID_PARAM;
    }

    (void)snprintf(path, sizeof(path), "%s/gpio%u/direction", GPIO_SYSFS_PATH, config->pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        LOG_ERROR("Failed to open direction for GPIO pin %u: %s", config->pin, strerror(errno));
        return HAL_ERROR;
    }

    written = write(fd, direction, strlen(direction));
    close(fd);
    if (written < 0) {
        LOG_ERROR("Failed to set direction for GPIO pin %u: %s", config->pin, strerror(errno));
        return HAL_ERROR;
    }

    if (config->pin <= 255) {
        configured_pins[config->pin] = 1;
    }

    return HAL_OK;
}

hal_status_t gpio_set(uint32_t pin, gpio_level_t level)
{
    char path[64];
    int fd;
    const char *value;
    ssize_t written;

    if (level > GPIO_LEVEL_HIGH) {
        LOG_ERROR("Invalid GPIO level: %d", level);
        return HAL_INVALID_PARAM;
    }

    value = (level == GPIO_LEVEL_HIGH) ? "1" : "0";
    (void)snprintf(path, sizeof(path), "%s/gpio%u/value", GPIO_SYSFS_PATH, pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        LOG_ERROR("Failed to open value for GPIO pin %u: %s", pin, strerror(errno));
        return HAL_ERROR;
    }

    written = write(fd, value, 1);
    close(fd);
    if (written < 0) {
        LOG_ERROR("Failed to write value for GPIO pin %u: %s", pin, strerror(errno));
        return HAL_ERROR;
    }

    return HAL_OK;
}

hal_status_t gpio_read(uint32_t pin, gpio_level_t *level)
{
    char path[64];
    int fd;
    char value;
    ssize_t read_bytes;

    if (level == NULL) {
        LOG_ERROR("GPIO read failed: level pointer is NULL");
        return HAL_INVALID_PARAM;
    }

    (void)snprintf(path, sizeof(path), "%s/gpio%u/value", GPIO_SYSFS_PATH, pin);
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        LOG_ERROR("Failed to open value for GPIO pin %u: %s", pin, strerror(errno));
        return HAL_ERROR;
    }

    read_bytes = read(fd, &value, 1);
    close(fd);
    if (read_bytes <= 0) {
        LOG_ERROR("Failed to read value for GPIO pin %u", pin);
        return HAL_ERROR;
    }

    *level = (value == '1') ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW;
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
    uint32_t pin;

    LOG_INFO("Deinitializing GPIO subsystem");
    for (pin = 0; pin < 256; pin++) {
        if (configured_pins[pin]) {
            (void)gpio_unexport(pin);
            configured_pins[pin] = 0;
        }
    }
    return HAL_OK;
}

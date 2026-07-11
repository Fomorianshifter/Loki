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

#ifdef HAVE_LIBGPIOD
#include <gpiod.h>
#endif

static const char *GPIO_SYSFS_PATH = "/sys/class/gpio";
static uint8_t configured_pins[256] = {0};

#ifdef HAVE_LIBGPIOD
typedef struct {
    uint8_t configured;
    gpio_mode_t mode;
    struct gpiod_line_request *request;
} gpio_line_state_t;

static struct gpiod_chip *gpio_chip = NULL;
static gpio_line_state_t gpio_lines[256] = {0};
#endif

/* ===== LOCAL FUNCTIONS ===== */

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

#ifdef HAVE_LIBGPIOD
static hal_status_t gpio_ensure_chip(void)
{
    if (gpio_chip != NULL) {
        return HAL_OK;
    }

    gpio_chip = gpiod_chip_open("/dev/gpiochip0");
    if (gpio_chip == NULL) {
        LOG_ERROR("Failed to open /dev/gpiochip0 for GPIO control");
        return HAL_ERROR;
    }

    return HAL_OK;
}

static enum gpiod_line_bias gpio_pull_to_bias(gpio_pull_t pull)
{
    switch (pull) {
        case GPIO_PULL_UP:
            return GPIOD_LINE_BIAS_PULL_UP;
        case GPIO_PULL_DOWN:
            return GPIOD_LINE_BIAS_PULL_DOWN;
        case GPIO_PULL_NONE:
        default:
            return GPIOD_LINE_BIAS_DISABLED;
    }
}

static hal_status_t gpio_configure_line_libgpiod(const gpio_config_t *config)
{
    struct gpiod_line_settings *settings = NULL;
    struct gpiod_line_config *line_cfg = NULL;
    struct gpiod_request_config *req_cfg = NULL;
    struct gpiod_line_request *request = NULL;
    unsigned int offset = config->pin;
    enum gpiod_line_direction direction;

    if (gpio_ensure_chip() != HAL_OK) {
        return HAL_ERROR;
    }

    if (gpio_lines[config->pin].request != NULL) {
        gpiod_line_request_release(gpio_lines[config->pin].request);
        gpio_lines[config->pin].request = NULL;
    }

    if (config->mode == GPIO_MODE_INPUT) {
        direction = GPIOD_LINE_DIRECTION_INPUT;
    } else if (config->mode == GPIO_MODE_OUTPUT) {
        direction = GPIOD_LINE_DIRECTION_OUTPUT;
    } else {
        LOG_ERROR("GPIO alternate mode is not supported");
        return HAL_NOT_SUPPORTED;
    }

    settings = gpiod_line_settings_new();
    line_cfg = gpiod_line_config_new();
    req_cfg = gpiod_request_config_new();
    if (settings == NULL || line_cfg == NULL || req_cfg == NULL) {
        LOG_ERROR("Failed to allocate libgpiod line configuration for pin %u", config->pin);
        goto error;
    }

    if (gpiod_line_settings_set_direction(settings, direction) < 0) {
        LOG_ERROR("Failed to set GPIO direction for pin %u", config->pin);
        goto error;
    }

    if (gpiod_line_settings_set_bias(settings, gpio_pull_to_bias(config->pull)) < 0) {
        LOG_ERROR("Failed to set GPIO pull for pin %u", config->pin);
        goto error;
    }

    if (gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings) < 0) {
        LOG_ERROR("Failed to build GPIO line config for pin %u", config->pin);
        goto error;
    }

    gpiod_request_config_set_consumer(req_cfg, "loki-gpio");
    request = gpiod_chip_request_lines(gpio_chip, req_cfg, line_cfg);
    if (request == NULL) {
        LOG_ERROR("Failed to request GPIO pin %u via libgpiod", config->pin);
        goto error;
    }

    gpio_lines[config->pin].request = request;
    gpio_lines[config->pin].mode = config->mode;
    gpio_lines[config->pin].configured = 1;
    configured_pins[config->pin] = 1;

    gpiod_line_settings_free(settings);
    gpiod_line_config_free(line_cfg);
    gpiod_request_config_free(req_cfg);
    return HAL_OK;

error:
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(line_cfg);
    gpiod_request_config_free(req_cfg);
    if (request != NULL) {
        gpiod_line_request_release(request);
    }
    return HAL_ERROR;
}
#endif

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t gpio_init(void)
{
    LOG_INFO("Initializing GPIO subsystem");
#ifdef HAVE_LIBGPIOD
    if (gpio_ensure_chip() != HAL_OK) {
        return HAL_ERROR;
    }
#endif
    return HAL_OK;
}

hal_status_t gpio_configure(const gpio_config_t *config)
{
#ifndef HAVE_LIBGPIOD
    char path[64];
    int fd;
    const char *direction;
    ssize_t written;
#endif

    if (config == NULL) {
        LOG_ERROR("GPIO configuration failed: config is NULL");
        return HAL_INVALID_PARAM;
    }

    if (config->pin > 255) {
        return HAL_INVALID_PARAM;
    }

    LOG_DEBUG("Configuring GPIO pin %u (mode=%d, pull=%d)",
              config->pin, config->mode, config->pull);

#ifdef HAVE_LIBGPIOD
    return gpio_configure_line_libgpiod(config);
#else
    if (gpio_export(config->pin) != HAL_OK) {
        LOG_ERROR("Failed to export GPIO pin %u", config->pin);
        return HAL_ERROR;
    }

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

    configured_pins[config->pin] = 1;
    return HAL_OK;
#endif
}

hal_status_t gpio_set(uint32_t pin, gpio_level_t level)
{
#ifdef HAVE_LIBGPIOD
    int result;
#else
    char path[64];
    int fd;
    const char *value;
    ssize_t written;
#endif

    if (pin > 255) {
        return HAL_INVALID_PARAM;
    }

    if (level > GPIO_LEVEL_HIGH) {
        LOG_ERROR("Invalid GPIO level: %d", level);
        return HAL_INVALID_PARAM;
    }

#ifdef HAVE_LIBGPIOD
    if (!gpio_lines[pin].configured || gpio_lines[pin].request == NULL) {
        LOG_ERROR("GPIO pin %u is not configured", pin);
        return HAL_NOT_READY;
    }
    if (gpio_lines[pin].mode != GPIO_MODE_OUTPUT) {
        LOG_ERROR("GPIO pin %u is not configured as output", pin);
        return HAL_INVALID_PARAM;
    }

    result = gpiod_line_request_set_value(
            gpio_lines[pin].request,
            pin,
            (level == GPIO_LEVEL_HIGH) ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
    if (result < 0) {
        LOG_ERROR("Failed to set GPIO pin %u via libgpiod", pin);
        return HAL_ERROR;
    }
    return HAL_OK;
#else
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
#endif
}

hal_status_t gpio_read(uint32_t pin, gpio_level_t *level)
{
#ifdef HAVE_LIBGPIOD
    int value;
#else
    char path[64];
    int fd;
    char value;
    ssize_t read_bytes;
#endif

    if (pin > 255) {
        return HAL_INVALID_PARAM;
    }

    if (level == NULL) {
        LOG_ERROR("GPIO read failed: level pointer is NULL");
        return HAL_INVALID_PARAM;
    }

#ifdef HAVE_LIBGPIOD
    if (!gpio_lines[pin].configured || gpio_lines[pin].request == NULL) {
        LOG_ERROR("GPIO pin %u is not configured", pin);
        return HAL_NOT_READY;
    }

    value = gpiod_line_request_get_value(gpio_lines[pin].request, pin);
    if (value < 0) {
        LOG_ERROR("Failed to read GPIO pin %u via libgpiod", pin);
        return HAL_ERROR;
    }

    *level = (value == GPIOD_LINE_VALUE_ACTIVE) ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW;
    return HAL_OK;
#else
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
#endif
}

hal_status_t gpio_toggle(uint32_t pin)
{
    gpio_level_t current;
    hal_status_t status;

    LOG_DEBUG("Toggling GPIO pin %u", pin);

    status = gpio_read(pin, &current);
    if (status != HAL_OK) {
        LOG_ERROR("Failed to read GPIO pin %u for toggle", pin);
        return status;
    }

    return gpio_set(pin, (current == GPIO_LEVEL_HIGH) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH);
}

hal_status_t gpio_deinit(void)
{
    uint32_t pin;

    LOG_INFO("Deinitializing GPIO subsystem");
    for (pin = 0; pin < 256; pin++) {
        if (!configured_pins[pin]) {
            continue;
        }

#ifdef HAVE_LIBGPIOD
        if (gpio_lines[pin].request != NULL) {
            gpiod_line_request_release(gpio_lines[pin].request);
            gpio_lines[pin].request = NULL;
        }
        gpio_lines[pin].configured = 0;
        gpio_lines[pin].mode = GPIO_MODE_INPUT;
#else
        (void)gpio_unexport(pin);
#endif
        configured_pins[pin] = 0;
    }

#ifdef HAVE_LIBGPIOD
    if (gpio_chip != NULL) {
        gpiod_chip_close(gpio_chip);
        gpio_chip = NULL;
    }
#endif
    return HAL_OK;
}

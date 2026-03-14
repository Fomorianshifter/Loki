/**
 * TFT Display Driver Implementation for ILI9488
 * Orange Pi Zero 2W - SPI0 Interface
 */

#include "tft_driver.h"
#include "spi.h"
#include "gpio.h"
#include "pwm.h"
#include "pinout.h"
#include "platform.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

/* ===== ILI9488 COMMANDS ===== */
#define ILI9488_SWRESET         0x01
#define ILI9488_SLPOUT          0x11
#define ILI9488_DISPOFF         0x28
#define ILI9488_DISPON          0x29
#define ILI9488_CASET           0x2A  /* Set column address */
#define ILI9488_PASET           0x2B  /* Set page address */
#define ILI9488_RAMWR           0x2C  /* Write to RAM */
#define ILI9488_MADCTL          0x36  /* Memory access control */
#define ILI9488_COLMOD          0x3A  /* Interface pixel format */

/* ===== TFT STATE ===== */
typedef struct {
    uint8_t initialized;
    uint8_t rotation;
    uint8_t brightness;
    uint8_t backend;
#if PLATFORM == PLATFORM_LINUX
    int fb_fd;
    uint8_t *fb_ptr;
    size_t fb_size;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_line_length;
    uint32_t fb_bits_per_pixel;
#endif
} tft_context_t;

#define TFT_BACKEND_SPI         1U
#define TFT_BACKEND_FRAMEBUFFER 2U

static tft_context_t tft_ctx = {
    .initialized = 0,
    .rotation = TFT_ROTATION,
    .brightness = TFT_BRIGHTNESS,
    .backend = 0,
#if PLATFORM == PLATFORM_LINUX
    .fb_fd = -1,
    .fb_ptr = NULL,
    .fb_size = 0U,
    .fb_width = 0U,
    .fb_height = 0U,
    .fb_line_length = 0U,
    .fb_bits_per_pixel = 0U,
#endif
};

/* ===== LOCAL HELPER FUNCTIONS ===== */

/**
 * Send command byte to TFT
 */
static hal_status_t tft_write_command(uint8_t cmd)
{
    /* DC pin = LOW for command */
    gpio_set(GPIO_TFT_DC, GPIO_LEVEL_LOW);
    
    /* Send command byte */
    hal_status_t status = spi_write(SPI_BUS_0, SPI0_CS0, &cmd, 1);
    
    return status;
}

/**
 * Send data bytes to TFT
 */
static hal_status_t tft_write_data(const uint8_t *data, uint32_t length)
{
    /* DC pin = HIGH for data */
    gpio_set(GPIO_TFT_DC, GPIO_LEVEL_HIGH);
    
    /* Send data bytes */
    hal_status_t status = spi_write(SPI_BUS_0, SPI0_CS0, data, length);
    
    return status;
}

/**
 * Delay in milliseconds
 */
static void delay_ms(uint32_t ms)
{
    DELAY_US(ms * 1000);
}

/**
 * Issue display reset
 */
static void tft_reset(void)
{
    /* Pull RST low */
    gpio_set(GPIO_TFT_RST, GPIO_LEVEL_LOW);
    delay_ms(10);
    
    /* Pull RST high */
    gpio_set(GPIO_TFT_RST, GPIO_LEVEL_HIGH);
    delay_ms(100);
}

/**
 * Set address window for pixel writing
 */
static hal_status_t tft_set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t cmd_data[4];

    /* Set column address */
    tft_write_command(ILI9488_CASET);
    cmd_data[0] = (x0 >> 8) & 0xFF;
    cmd_data[1] = x0 & 0xFF;
    cmd_data[2] = (x1 >> 8) & 0xFF;
    cmd_data[3] = x1 & 0xFF;
    tft_write_data(cmd_data, 4);

    /* Set row address */
    tft_write_command(ILI9488_PASET);
    cmd_data[0] = (y0 >> 8) & 0xFF;
    cmd_data[1] = y0 & 0xFF;
    cmd_data[2] = (y1 >> 8) & 0xFF;
    cmd_data[3] = y1 & 0xFF;
    tft_write_data(cmd_data, 4);

    return HAL_OK;
}

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
static const char *tft_get_env(const char *name, const char *fallback)
{
    const char *value = getenv(name);
    if (value == NULL || value[0] == '\0') {
        return fallback;
    }
    return value;
}

static void tft_framebuffer_write_pixel(uint32_t x, uint32_t y, color_t color)
{
    uint8_t *pixel_address;
    uint16_t color_16;

    if (x >= tft_ctx.fb_width || y >= tft_ctx.fb_height || tft_ctx.fb_ptr == NULL) {
        return;
    }

    pixel_address = tft_ctx.fb_ptr + (y * tft_ctx.fb_line_length) + ((x * tft_ctx.fb_bits_per_pixel) / 8U);
    color_16 = color;

    if (tft_ctx.fb_bits_per_pixel == 16U) {
        memcpy(pixel_address, &color_16, sizeof(color_16));
    } else if (tft_ctx.fb_bits_per_pixel == 32U) {
        uint8_t red = (uint8_t)(((color >> 11) & 0x1FU) << 3);
        uint8_t green = (uint8_t)(((color >> 5) & 0x3FU) << 2);
        uint8_t blue = (uint8_t)((color & 0x1FU) << 3);

        pixel_address[0] = blue;
        pixel_address[1] = green;
        pixel_address[2] = red;
        pixel_address[3] = 0x00;
    }
}

static hal_status_t tft_framebuffer_init(const char *device_path)
{
    struct fb_fix_screeninfo fixed_info;
    struct fb_var_screeninfo variable_info;

    LOG_INFO("Attempting framebuffer display backend on %s", device_path);
    tft_ctx.fb_fd = open(device_path, O_RDWR);
    if (tft_ctx.fb_fd < 0) {
        LOG_WARN("Failed to open framebuffer device %s (errno=%d)", device_path, errno);
        return HAL_ERROR;
    }

    if (ioctl(tft_ctx.fb_fd, FBIOGET_FSCREENINFO, &fixed_info) != 0 ||
        ioctl(tft_ctx.fb_fd, FBIOGET_VSCREENINFO, &variable_info) != 0) {
        LOG_WARN("Failed to query framebuffer info for %s (errno=%d)", device_path, errno);
        close(tft_ctx.fb_fd);
        tft_ctx.fb_fd = -1;
        return HAL_ERROR;
    }

    tft_ctx.fb_size = fixed_info.smem_len;
    tft_ctx.fb_line_length = fixed_info.line_length;
    tft_ctx.fb_bits_per_pixel = variable_info.bits_per_pixel;
    tft_ctx.fb_width = variable_info.xres;
    tft_ctx.fb_height = variable_info.yres;
    tft_ctx.fb_ptr = mmap(NULL, tft_ctx.fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, tft_ctx.fb_fd, 0);
    if (tft_ctx.fb_ptr == MAP_FAILED) {
        LOG_WARN("Failed to map framebuffer %s (errno=%d)", device_path, errno);
        tft_ctx.fb_ptr = NULL;
        close(tft_ctx.fb_fd);
        tft_ctx.fb_fd = -1;
        return HAL_ERROR;
    }

    tft_ctx.backend = TFT_BACKEND_FRAMEBUFFER;
    tft_ctx.initialized = 1;
    LOG_INFO("Framebuffer ready on %s (%ux%u @ %u bpp)",
             device_path,
             (unsigned int)tft_ctx.fb_width,
             (unsigned int)tft_ctx.fb_height,
             (unsigned int)tft_ctx.fb_bits_per_pixel);
    return HAL_OK;
}
#endif

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t tft_init(void)
{
    const char *backend_name;

    if (tft_ctx.initialized) {
        return HAL_OK;
    }

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
    backend_name = tft_get_env("LOKI_DISPLAY_BACKEND", "auto");
    if (strcmp(backend_name, "framebuffer") == 0 || strcmp(backend_name, "auto") == 0) {
        const char *fb_device = tft_get_env("LOKI_FB_DEVICE", "/dev/fb1");
        if (tft_framebuffer_init(fb_device) == HAL_OK) {
            tft_clear();
            return HAL_OK;
        }

        if (strcmp(backend_name, "framebuffer") == 0) {
            LOG_WARN("Framebuffer backend requested explicitly and initialization failed");
            return HAL_ERROR;
        }

        LOG_WARN("Primary framebuffer backend failed, trying /dev/fb0 fallback");
        if (strcmp(fb_device, "/dev/fb0") != 0 && tft_framebuffer_init("/dev/fb0") == HAL_OK) {
            tft_clear();
            return HAL_OK;
        }
    }
#else
    backend_name = "spi";
#endif

    (void)backend_name;

    /* Initialize SPI0 for TFT */
    spi_config_t spi_cfg = {
        .frequency = TFT_SPI_FREQ,
        .mode = SPI_MODE_0,
        .bits_per_word = 8,
        .bit_order = SPI_MSB_FIRST,
    };
    
    if (spi_init(SPI_BUS_0, &spi_cfg) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Initialize GPIO for TFT control pins */
    gpio_config_t gpio_dc = {
        .pin = GPIO_TFT_DC,
        .mode = GPIO_MODE_OUTPUT,
        .pull = GPIO_PULL_NONE,
    };
    gpio_configure(&gpio_dc);

    gpio_config_t gpio_rst = {
        .pin = GPIO_TFT_RST,
        .mode = GPIO_MODE_OUTPUT,
        .pull = GPIO_PULL_NONE,
    };
    gpio_configure(&gpio_rst);

    /* Initialize PWM for backlight */
    pwm_config_t pwm_cfg = {
        .pin = GPIO_TFT_BL,
        .frequency = PWM_FREQ_DEFAULT,
        .duty_cycle = tft_ctx.brightness,
    };
    pwm_init(PWM_CHANNEL_0, &pwm_cfg);
    pwm_enable(PWM_CHANNEL_0);

    /* Reset display */
    tft_reset();

    /* Initialize ILI9488 controller */
    
    /* Software reset */
    tft_write_command(ILI9488_SWRESET);
    delay_ms(50);

    /* Sleep out */
    tft_write_command(ILI9488_SLPOUT);
    delay_ms(100);

    /* Color mode: 16-bit RGB565 */
    tft_write_command(ILI9488_COLMOD);
    uint8_t colmod_data = 0x55;  /* 16-bit/pixel */
    tft_write_data(&colmod_data, 1);

    /* Memory access control */
    tft_write_command(ILI9488_MADCTL);
    uint8_t madctl_data = 0x00;  /* Default orientation */
    tft_write_data(&madctl_data, 1);

    /* Display on */
    tft_write_command(ILI9488_DISPON);
    delay_ms(100);

    /* Clear display */
    tft_clear();

    tft_ctx.backend = TFT_BACKEND_SPI;
    tft_ctx.initialized = 1;
    return HAL_OK;
}

hal_status_t tft_write_pixels(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                              const color_t *data)
{
    if (data == NULL || width == 0 || height == 0) {
        return HAL_INVALID_PARAM;
    }

    if (!tft_ctx.initialized) {
        return HAL_NOT_READY;
    }

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
    if (tft_ctx.backend == TFT_BACKEND_FRAMEBUFFER) {
        uint32_t row;
        uint32_t column;
        uint32_t target_width = width;
        uint32_t target_height = height;

        if ((uint32_t)x + target_width > tft_ctx.fb_width) {
            target_width = tft_ctx.fb_width - x;
        }
        if ((uint32_t)y + target_height > tft_ctx.fb_height) {
            target_height = tft_ctx.fb_height - y;
        }

        for (row = 0; row < target_height; row++) {
            for (column = 0; column < target_width; column++) {
                tft_framebuffer_write_pixel(x + column, y + row, data[(row * width) + column]);
            }
        }

        return HAL_OK;
    }
#endif

    /* Set address window */
    tft_set_address_window(x, y, x + width - 1, y + height - 1);

    /* Write pixel data */
    tft_write_command(ILI9488_RAMWR);
    
    uint32_t pixel_count = width * height;
    uint8_t *pixel_data = (uint8_t *)data;
    uint32_t data_length = pixel_count * 2;  /* 2 bytes per pixel in RGB565 */
    
    tft_write_data(pixel_data, data_length);

    return HAL_OK;
}

hal_status_t tft_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, color_t color)
{
    if (width == 0 || height == 0) {
        return HAL_INVALID_PARAM;
    }

    if (!tft_ctx.initialized) {
        return HAL_NOT_READY;
    }

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
    if (tft_ctx.backend == TFT_BACKEND_FRAMEBUFFER) {
        uint32_t row;
        uint32_t column;
        uint32_t target_width = width;
        uint32_t target_height = height;

        if ((uint32_t)x + target_width > tft_ctx.fb_width) {
            target_width = tft_ctx.fb_width - x;
        }
        if ((uint32_t)y + target_height > tft_ctx.fb_height) {
            target_height = tft_ctx.fb_height - y;
        }

        for (row = 0; row < target_height; row++) {
            for (column = 0; column < target_width; column++) {
                tft_framebuffer_write_pixel(x + column, y + row, color);
            }
        }

        return HAL_OK;
    }
#endif

    /* Set address window */
    tft_set_address_window(x, y, x + width - 1, y + height - 1);

    /* Write command and prepare for data */
    tft_write_command(ILI9488_RAMWR);

    /* Send color data repeatedly */
    uint32_t pixel_count = width * height;
    
    /* Convert color to bytes (RGB565: RRRRRGGGGGGBBBBBs) */
    uint8_t color_bytes[2] = {
        (color >> 8) & 0xFF,  /* High byte */
        color & 0xFF,          /* Low byte */
    };

    /* Write same color for all pixels */
    for (uint32_t i = 0; i < pixel_count; i++) {
        tft_write_data(color_bytes, 2);
    }

    return HAL_OK;
}

hal_status_t tft_clear(void)
{
    return tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_BLACK);
}

hal_status_t tft_set_brightness(uint8_t brightness)
{
    if (brightness > 100) {
        brightness = 100;
    }

    tft_ctx.brightness = brightness;

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
    if (tft_ctx.backend == TFT_BACKEND_FRAMEBUFFER) {
        return HAL_OK;
    }
#endif

    return pwm_set_duty(PWM_CHANNEL_0, brightness);
}

hal_status_t tft_set_rotation(uint8_t rotation)
{
    if (rotation > 3) {
        return HAL_INVALID_PARAM;
    }

    if (!tft_ctx.initialized) {
        return HAL_NOT_READY;
    }

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
    if (tft_ctx.backend == TFT_BACKEND_FRAMEBUFFER) {
        tft_ctx.rotation = rotation;
        return HAL_OK;
    }
#endif

    tft_ctx.rotation = rotation;

    /* Set MADCTL register based on rotation */
    tft_write_command(ILI9488_MADCTL);
    
    uint8_t madctl = 0x00;
    switch (rotation) {
        case 0: madctl = 0x00; break;  /* 0° */
        case 1: madctl = 0x60; break;  /* 90° */
        case 2: madctl = 0xC0; break;  /* 180° */
        case 3: madctl = 0xA0; break;  /* 270° */
    }
    
    tft_write_data(&madctl, 1);

    return HAL_OK;
}

hal_status_t tft_deinit(void)
{
    if (!tft_ctx.initialized) {
        return HAL_OK;
    }

#if PLATFORM == PLATFORM_LINUX && !defined(_WIN32)
    if (tft_ctx.backend == TFT_BACKEND_FRAMEBUFFER) {
        if (tft_ctx.fb_ptr != NULL) {
            munmap(tft_ctx.fb_ptr, tft_ctx.fb_size);
            tft_ctx.fb_ptr = NULL;
        }
        if (tft_ctx.fb_fd >= 0) {
            close(tft_ctx.fb_fd);
            tft_ctx.fb_fd = -1;
        }
        tft_ctx.initialized = 0;
        tft_ctx.backend = 0;
        return HAL_OK;
    }
#endif

    /* Display off */
    tft_write_command(ILI9488_DISPOFF);

    /* Disable backlight */
    pwm_disable(PWM_CHANNEL_0);
    pwm_deinit(PWM_CHANNEL_0);

    /* Deinitialize SPI */
    spi_deinit(SPI_BUS_0);

    tft_ctx.initialized = 0;
    return HAL_OK;
}

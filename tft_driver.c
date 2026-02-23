/**
 * TFT Display Driver Implementation for ILI9488
 * Orange Pi Zero 2W - SPI0 Interface
 */

#include "tft_driver.h"
#include "../../hal/spi/spi.h"
#include "../../hal/gpio/gpio.h"
#include "../../hal/pwm/pwm.h"
#include "../../config/pinout.h"
#include <stdio.h>
#include <unistd.h>

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
} tft_context_t;

static tft_context_t tft_ctx = {
    .initialized = 0,
    .rotation = TFT_ROTATION,
    .brightness = TFT_BRIGHTNESS,
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
    usleep(ms * 1000);
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

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t tft_init(void)
{
    if (tft_ctx.initialized) {
        return HAL_OK;
    }

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

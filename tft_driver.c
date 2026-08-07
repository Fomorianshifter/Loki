/**
 * TFT Display Driver Implementation for ILI9488
 * Raspberry Pi - SPI0 Interface
 */

#include "tft_driver.h"
#include "spi.h"
#include "gpio.h"
#include "pwm.h"
#include "pinout.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* ===== COMMON ILI948x COMMANDS ===== */
#define ILI948X_SWRESET         0x01
#define ILI948X_SLPOUT          0x11
#define ILI948X_DISPOFF         0x28
#define ILI948X_DISPON          0x29
#define ILI948X_CASET           0x2A  /* Set column address */
#define ILI948X_PASET           0x2B  /* Set page address */
#define ILI948X_RAMWR           0x2C  /* Write to RAM */
#define ILI948X_MADCTL          0x36  /* Memory access control */
#define ILI948X_COLMOD          0x3A  /* Interface pixel format */

/* ===== ILI9488 COMMANDS ===== */
#define ILI9488_SWRESET         ILI948X_SWRESET
#define ILI9488_SLPOUT          ILI948X_SLPOUT
#define ILI9488_DISPOFF         ILI948X_DISPOFF
#define ILI9488_DISPON          ILI948X_DISPON
#define ILI9488_CASET           ILI948X_CASET
#define ILI9488_PASET           ILI948X_PASET
#define ILI9488_RAMWR           ILI948X_RAMWR
#define ILI9488_MADCTL          ILI948X_MADCTL
#define ILI9488_COLMOD          ILI948X_COLMOD

/* ===== ILI9486-SPECIFIC COMMANDS ===== */
#define ILI9486_IFMODE          0xB0
#define ILI9486_FRMCTR1         0xB1
#define ILI9486_DISCTRL         0xB6
#define ILI9486_PWCTRL1         0xC0
#define ILI9486_PWCTRL2         0xC1
#define ILI9486_PWCTRL3         0xC2
#define ILI9486_VMCTRL1         0xC5

/* ===== TFT STATE ===== */
typedef struct {
    uint8_t initialized;
    uint8_t rotation;
    uint8_t brightness;
    uint8_t backlight_ready;
    uint8_t uses_18bit_pixels;
} tft_context_t;

static tft_context_t tft_ctx = {
    .initialized = 0,
    .rotation = TFT_ROTATION,
    .brightness = TFT_BRIGHTNESS,
    .backlight_ready = 0,
    .uses_18bit_pixels = 0,
};

/* ===== LOCAL HELPER FUNCTIONS ===== */

/**
 * Send command byte to TFT
 */
static hal_status_t tft_write_command(uint8_t cmd)
{
    hal_status_t status;

    /* DC pin = LOW for command */
    status = gpio_set(GPIO_TFT_DC, GPIO_LEVEL_LOW);
    if (status != HAL_OK) {
        LOG_ERROR("TFT command phase failed: unable to set DC pin %u low", GPIO_TFT_DC);
        return status;
    }
    
    /* Send command byte */
    status = spi_write(SPI_BUS_0, TFT_CS, &cmd, 1);
    if (status != HAL_OK) {
        LOG_ERROR("TFT command write failed (cmd=0x%02X)", cmd);
    }
    
    return status;
}

/**
 * Send data bytes to TFT
 */
static hal_status_t tft_write_data(const uint8_t *data, uint32_t length)
{
    hal_status_t status;

    /* DC pin = HIGH for data */
    status = gpio_set(GPIO_TFT_DC, GPIO_LEVEL_HIGH);
    if (status != HAL_OK) {
        LOG_ERROR("TFT data phase failed: unable to set DC pin %u high", GPIO_TFT_DC);
        return status;
    }
    
    /* Send data bytes */
    status = spi_write(SPI_BUS_0, TFT_CS, data, length);
    if (status != HAL_OK) {
        LOG_ERROR("TFT data write failed (len=%u)", length);
    }
    
    return status;
}

/**
 * Delay in milliseconds
 */
static void delay_ms(uint32_t ms)
{
    usleep(ms * 1000);
}

static void rgb565_to_rgb666_bytes(color_t color, uint8_t *rgb)
{
    uint8_t r5 = (uint8_t)((color >> 11) & 0x1F);
    uint8_t g6 = (uint8_t)((color >> 5) & 0x3F);
    uint8_t b5 = (uint8_t)(color & 0x1F);

    /* Expand RGB565 to 8-bit channels for ILI9488 18-bit SPI writes. */
    rgb[0] = (uint8_t)((r5 << 3) | (r5 >> 2));
    rgb[1] = (uint8_t)((g6 << 2) | (g6 >> 4));
    rgb[2] = (uint8_t)((b5 << 3) | (b5 >> 2));
}

static void rgb565_to_rgb565_bytes(color_t color, uint8_t *rgb)
{
    rgb[0] = (uint8_t)(color >> 8);
    rgb[1] = (uint8_t)(color & 0xFF);
}

/**
 * Issue display reset
 */
static void tft_reset(void)
{
    /* Pull RST low */
    (void)gpio_set(GPIO_TFT_RST, GPIO_LEVEL_LOW);
    delay_ms(10);
    
    /* Pull RST high */
    (void)gpio_set(GPIO_TFT_RST, GPIO_LEVEL_HIGH);
    delay_ms(100);
}

static void tft_init_sequence_ili9486(void)
{
    uint8_t data[4];

    tft_write_command(ILI9486_IFMODE);
    data[0] = 0x00;
    tft_write_data(data, 1);

    tft_write_command(ILI9486_FRMCTR1);
    data[0] = 0xB0;
    data[1] = 0x11;
    tft_write_data(data, 2);

    tft_write_command(ILI9486_DISCTRL);
    data[0] = 0x02;
    data[1] = 0x42;
    tft_write_data(data, 2);

    tft_write_command(ILI9486_PWCTRL1);
    data[0] = 0x19;
    data[1] = 0x1A;
    tft_write_data(data, 2);

    tft_write_command(ILI9486_PWCTRL2);
    data[0] = 0x45;
    tft_write_data(data, 1);

    tft_write_command(ILI9486_PWCTRL3);
    data[0] = 0x33;
    tft_write_data(data, 1);

    tft_write_command(ILI9486_VMCTRL1);
    data[0] = 0x00;
    data[1] = 0x28;
    tft_write_data(data, 2);

    delay_ms(5);
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
    hal_status_t status;

    if (tft_ctx.initialized) {
        return HAL_OK;
    }

    LOG_INFO("TFT init: controller=%s  %ux%u  SPI_FREQ=%u Hz  CS=%u  DC=%u  RST=%u  BL=%u  rot=%u  brightness=%u%%",
             TFT_TYPE,
             (unsigned)TFT_WIDTH, (unsigned)TFT_HEIGHT,
             (unsigned)TFT_SPI_FREQ,
             (unsigned)TFT_CS,
             (unsigned)GPIO_TFT_DC,
             (unsigned)GPIO_TFT_RST,
             (unsigned)GPIO_TFT_BL,
             (unsigned)TFT_ROTATION,
             (unsigned)TFT_BRIGHTNESS);

    /* Initialize SPI0 for TFT */
    spi_config_t spi_cfg = {
        .frequency = TFT_SPI_FREQ,
        .mode = SPI_MODE_0,
        .bits_per_word = 8,
        .bit_order = SPI_MSB_FIRST,
        .cs_line = (TFT_CS == SPI0_CS0) ? 0u : 1u,
    };
    
    status = spi_init(SPI_BUS_0, &spi_cfg);
    if (status != HAL_OK) {
        LOG_ERROR("TFT init failed: SPI0 init failed");
        return HAL_ERROR;
    }

    /* Initialize GPIO for TFT control pins */
    gpio_config_t gpio_dc = {
        .pin = GPIO_TFT_DC,
        .mode = GPIO_MODE_OUTPUT,
        .pull = GPIO_PULL_NONE,
    };
    status = gpio_configure(&gpio_dc);
    if (status != HAL_OK) {
        LOG_ERROR("TFT init failed: DC GPIO %u configure failed", GPIO_TFT_DC);
        return HAL_ERROR;
    }

    gpio_config_t gpio_rst = {
        .pin = GPIO_TFT_RST,
        .mode = GPIO_MODE_OUTPUT,
        .pull = GPIO_PULL_NONE,
    };
    status = gpio_configure(&gpio_rst);
    if (status != HAL_OK) {
        LOG_ERROR("TFT init failed: RST GPIO %u configure failed", GPIO_TFT_RST);
        return HAL_ERROR;
    }

    if (GPIO_TFT_BL == 255u) {
        LOG_INFO("Backlight control disabled (GPIO_TFT_BL=255)");
    } else {
        /* Initialize PWM for backlight */
        pwm_config_t pwm_cfg = {
            .pin = GPIO_TFT_BL,
            .frequency = PWM_FREQ_DEFAULT,
            .duty_cycle = tft_ctx.brightness,
        };
        status = pwm_init(PWM_CHANNEL_0, &pwm_cfg);
        if (status != HAL_OK) {
            LOG_WARN("Backlight PWM init failed on GPIO %u; continuing without backlight control", GPIO_TFT_BL);
        } else {
            status = pwm_enable(PWM_CHANNEL_0);
            if (status != HAL_OK) {
                LOG_WARN("Backlight PWM enable failed on GPIO %u; continuing without backlight control", GPIO_TFT_BL);
            } else {
                tft_ctx.backlight_ready = 1;
            }
        }
    }

    /* Reset display */
    tft_reset();

    /* Initialize ILI9488 controller */
    
    /* Software reset */
    if (tft_write_command(ILI9488_SWRESET) != HAL_OK) {
        LOG_ERROR("TFT init failed: SWRESET command");
        return HAL_ERROR;
    }
    delay_ms(150);

    /* Sleep out */
    if (tft_write_command(ILI9488_SLPOUT) != HAL_OK) {
        LOG_ERROR("TFT init failed: SLPOUT command");
        return HAL_ERROR;
    }
    delay_ms(120);

    if (strcmp(TFT_TYPE, "ILI9486") == 0) {
        tft_init_sequence_ili9486();
    }

    tft_ctx.uses_18bit_pixels = (strcmp(TFT_TYPE, "ILI9488") == 0) ? 1u : 0u;
    LOG_INFO("TFT controller profile: %s (%s pixel writes)",
             TFT_TYPE,
             tft_ctx.uses_18bit_pixels ? "18-bit" : "16-bit");

    /* ILI9488 SPI mode uses 18-bit pixel data; common ILI9486 panels use 16-bit. */
    if (tft_write_command(ILI9488_COLMOD) != HAL_OK) {
        LOG_ERROR("TFT init failed: COLMOD command");
        return HAL_ERROR;
    }
    uint8_t colmod_data = tft_ctx.uses_18bit_pixels ? 0x66u : 0x55u;
    if (tft_write_data(&colmod_data, 1) != HAL_OK) {
        LOG_ERROR("TFT init failed: COLMOD payload");
        return HAL_ERROR;
    }

    /* Memory access control */
    if (tft_write_command(ILI9488_MADCTL) != HAL_OK) {
        LOG_ERROR("TFT init failed: MADCTL command");
        return HAL_ERROR;
    }
    uint8_t madctl_data = 0x48;  /* Default landscape orientation */
    if (tft_write_data(&madctl_data, 1) != HAL_OK) {
        LOG_ERROR("TFT init failed: MADCTL payload");
        return HAL_ERROR;
    }

    /* Display on */
    if (tft_write_command(ILI9488_DISPON) != HAL_OK) {
        LOG_ERROR("TFT init failed: DISPON command");
        return HAL_ERROR;
    }
    delay_ms(100);

    /* Clear display */
    tft_ctx.initialized = 1;
    if (tft_clear() != HAL_OK) {
        tft_ctx.initialized = 0;
        LOG_ERROR("TFT init failed: initial clear");
        return HAL_ERROR;
    }
    return HAL_OK;
}

hal_status_t tft_write_pixels(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                              const color_t *data)
{
    uint8_t *pixel_data;
    uint32_t pixel_count;
    uint32_t i;

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

    pixel_count = (uint32_t)width * (uint32_t)height;
    if (tft_ctx.uses_18bit_pixels) {
        pixel_data = (uint8_t *)malloc(pixel_count * 3u);
    } else {
        pixel_data = (uint8_t *)malloc(pixel_count * 2u);
    }
    if (pixel_data == NULL) {
        return HAL_ERROR;
    }

    if (tft_ctx.uses_18bit_pixels) {
        for (i = 0; i < pixel_count; i++) {
            rgb565_to_rgb666_bytes(data[i], &pixel_data[i * 3u]);
        }
        if (tft_write_data(pixel_data, pixel_count * 3u) != HAL_OK) {
            free(pixel_data);
            return HAL_ERROR;
        }
    } else {
        for (i = 0; i < pixel_count; i++) {
            rgb565_to_rgb565_bytes(data[i], &pixel_data[i * 2u]);
        }
        if (tft_write_data(pixel_data, pixel_count * 2u) != HAL_OK) {
            free(pixel_data);
            return HAL_ERROR;
        }
    }
    free(pixel_data);

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

    uint32_t pixel_count = (uint32_t)width * (uint32_t)height;
    enum { TFT_FILL_CHUNK_PIXELS = 256 };

    if (tft_ctx.uses_18bit_pixels) {
        uint8_t color_bytes[3];
        uint8_t chunk[TFT_FILL_CHUNK_PIXELS * 3];
        rgb565_to_rgb666_bytes(color, color_bytes);
        for (uint32_t i = 0; i < TFT_FILL_CHUNK_PIXELS; i++) {
            chunk[i * 3u + 0] = color_bytes[0];
            chunk[i * 3u + 1] = color_bytes[1];
            chunk[i * 3u + 2] = color_bytes[2];
        }
        while (pixel_count > 0) {
            uint32_t pixels = (pixel_count > TFT_FILL_CHUNK_PIXELS) ? TFT_FILL_CHUNK_PIXELS : pixel_count;
            if (tft_write_data(chunk, pixels * 3u) != HAL_OK) {
                return HAL_ERROR;
            }
            pixel_count -= pixels;
        }
    } else {
        uint8_t color_bytes[2];
        uint8_t chunk[TFT_FILL_CHUNK_PIXELS * 2];
        rgb565_to_rgb565_bytes(color, color_bytes);
        for (uint32_t i = 0; i < TFT_FILL_CHUNK_PIXELS; i++) {
            chunk[i * 2u + 0] = color_bytes[0];
            chunk[i * 2u + 1] = color_bytes[1];
        }
        while (pixel_count > 0) {
            uint32_t pixels = (pixel_count > TFT_FILL_CHUNK_PIXELS) ? TFT_FILL_CHUNK_PIXELS : pixel_count;
            if (tft_write_data(chunk, pixels * 2u) != HAL_OK) {
                return HAL_ERROR;
            }
            pixel_count -= pixels;
        }
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
    if (!tft_ctx.backlight_ready) {
        return HAL_OK;
    }

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
    tft_write_command(ILI948X_MADCTL);
    
    uint8_t madctl = 0x00;
    switch (rotation) {
        case 0: madctl = 0x48; break;  /* 0° */
        case 1: madctl = 0x28; break;  /* 90° */
        case 2: madctl = 0x88; break;  /* 180° */
        case 3: madctl = 0xE8; break;  /* 270° */
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
    if (tft_ctx.backlight_ready) {
        pwm_disable(PWM_CHANNEL_0);
        pwm_deinit(PWM_CHANNEL_0);
        tft_ctx.backlight_ready = 0;
    }

    /* Deinitialize SPI */
    spi_deinit(SPI_BUS_0);

    tft_ctx.initialized = 0;
    return HAL_OK;
}

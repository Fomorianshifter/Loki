/**
 * TFT Display Driver Implementation for ILI9486 / ILI9488
 * Orange Pi Zero 2W - SPI0 Interface
 *
 * TFT_TYPE (from board_config.h / config.toml) selects the controller init path:
 *   "ILI9486" — extended power/VCOM sequence for common 480×320 modules
 *   "ILI9488" — shorter generic sequence (compatible default)
 *
 * CS line, DC/RST/BL pins, SPI frequency, and rotation are all sourced from
 * the generated config.h (board_config.h + pinout.h).
 */

#include "tft_driver.h"
#include "config.h"
#include "log.h"
#include "spi.h"
#include "gpio.h"
#include "pwm.h"
#include <string.h>
#include <unistd.h>

/* ===== COMMON ILI948x COMMANDS (shared by ILI9486 and ILI9488) ===== */
#define ILI948X_SWRESET     0x01  /* Software reset */
#define ILI948X_SLPOUT      0x11  /* Sleep out */
#define ILI948X_DISPOFF     0x28  /* Display off */
#define ILI948X_DISPON      0x29  /* Display on */
#define ILI948X_CASET       0x2A  /* Column address set */
#define ILI948X_PASET       0x2B  /* Page (row) address set */
#define ILI948X_RAMWR       0x2C  /* Memory write */
#define ILI948X_MADCTL      0x36  /* Memory access control (rotation/mirror) */
#define ILI948X_COLMOD      0x3A  /* Interface pixel format */

/* ===== ILI9486-SPECIFIC COMMANDS ===== */
#define ILI9486_IFMODE      0xB0  /* Interface mode control */
#define ILI9486_FRMCTR1     0xB1  /* Frame rate control (normal mode) */
#define ILI9486_DISCTRL     0xB6  /* Display function control */
#define ILI9486_PWCTRL1     0xC0  /* Power control 1 */
#define ILI9486_PWCTRL2     0xC1  /* Power control 2 */
#define ILI9486_PWCTRL3     0xC2  /* Power control 3 */
#define ILI9486_VMCTRL1     0xC5  /* VCOM control 1 */

/* ===== TFT STATE ===== */
typedef struct {
    uint8_t initialized;
    uint8_t rotation;
    uint8_t brightness;
} tft_context_t;

static tft_context_t tft_ctx = {
    .initialized = 0,
    .rotation    = TFT_ROTATION,
    .brightness  = TFT_BRIGHTNESS,
};

/* ===== LOCAL HELPER FUNCTIONS ===== */

/**
 * Send a command byte to the TFT controller (DC = LOW).
 */
static hal_status_t tft_write_command(uint8_t cmd)
{
    gpio_set(GPIO_TFT_DC, GPIO_LEVEL_LOW);
    return spi_write(SPI_BUS_0, TFT_CS, &cmd, 1);
}

/**
 * Send data bytes to the TFT controller (DC = HIGH).
 */
static hal_status_t tft_write_data(const uint8_t *data, uint32_t length)
{
    gpio_set(GPIO_TFT_DC, GPIO_LEVEL_HIGH);
    return spi_write(SPI_BUS_0, TFT_CS, data, length);
}

/**
 * Busy-wait in milliseconds.
 */
static void delay_ms(uint32_t ms)
{
    usleep(ms * 1000);
}

/**
 * Perform a hardware reset via the RST GPIO.
 */
static void tft_reset(void)
{
    gpio_set(GPIO_TFT_RST, GPIO_LEVEL_LOW);
    delay_ms(10);
    gpio_set(GPIO_TFT_RST, GPIO_LEVEL_HIGH);
    delay_ms(120);
}

/**
 * Set the pixel address window for subsequent RAMWR commands.
 */
static hal_status_t tft_set_address_window(uint16_t x0, uint16_t y0,
                                            uint16_t x1, uint16_t y1)
{
    uint8_t cmd_data[4];

    tft_write_command(ILI948X_CASET);
    cmd_data[0] = (x0 >> 8) & 0xFF;
    cmd_data[1] =  x0       & 0xFF;
    cmd_data[2] = (x1 >> 8) & 0xFF;
    cmd_data[3] =  x1       & 0xFF;
    tft_write_data(cmd_data, 4);

    tft_write_command(ILI948X_PASET);
    cmd_data[0] = (y0 >> 8) & 0xFF;
    cmd_data[1] =  y0       & 0xFF;
    cmd_data[2] = (y1 >> 8) & 0xFF;
    cmd_data[3] =  y1       & 0xFF;
    tft_write_data(cmd_data, 4);

    return HAL_OK;
}

/**
 * ILI9486 extended power / VCOM / frame-rate init sequence.
 * Called after SLPOUT to program controller-specific registers
 * that are not required (or differ) for ILI9488.
 */
static void tft_init_sequence_ili9486(void)
{
    uint8_t data[4];

    /* Interface mode: SDO not used, VSYNC/HSYNC disabled */
    tft_write_command(ILI9486_IFMODE);
    data[0] = 0x00;
    tft_write_data(data, 1);

    /* Frame rate: fosc / 1, 60 Hz */
    tft_write_command(ILI9486_FRMCTR1);
    data[0] = 0xB0;
    data[1] = 0x11;
    tft_write_data(data, 2);

    /* Display function control */
    tft_write_command(ILI9486_DISCTRL);
    data[0] = 0x02;
    data[1] = 0x42;
    tft_write_data(data, 2);

    /* Power control 1: GVDD = 4.45 V, DCA=0 */
    tft_write_command(ILI9486_PWCTRL1);
    data[0] = 0x19;
    data[1] = 0x1A;
    tft_write_data(data, 2);

    /* Power control 2: DDVDH = VCI×2 */
    tft_write_command(ILI9486_PWCTRL2);
    data[0] = 0x45;
    tft_write_data(data, 1);

    /* Power control 3: operational amplifier current */
    tft_write_command(ILI9486_PWCTRL3);
    data[0] = 0x33;
    tft_write_data(data, 1);

    /* VCOM control: VCOMH = 4.025 V, VCOML = −1.0 V */
    tft_write_command(ILI9486_VMCTRL1);
    data[0] = 0x00;
    data[1] = 0x28;
    tft_write_data(data, 2);

    delay_ms(5);
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t tft_init(void)
{
    if (tft_ctx.initialized) {
        return HAL_OK;
    }

    /* ---- Log effective compile-time TFT configuration ---- */
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

    /* ---- Initialize SPI0 for TFT ---- */
    /*
     * cs_line selects /dev/spidev0.0 (TFT_CS == SPI0_CS0) or
     * /dev/spidev0.1 (TFT_CS == SPI0_CS1) so the kernel drives the
     * correct hardware CE line.
     */
    spi_config_t spi_cfg = {
        .frequency    = TFT_SPI_FREQ,
        .mode         = SPI_MODE_0,
        .bits_per_word = 8,
        .bit_order    = SPI_MSB_FIRST,
        .cs_line      = (TFT_CS == SPI0_CS0) ? 0 : 1,
    };

    if (spi_init(SPI_BUS_0, &spi_cfg) != HAL_OK) {
        LOG_ERROR("TFT SPI init failed");
        return HAL_ERROR;
    }

    /* ---- Configure DC and RST GPIO ---- */
    gpio_config_t gpio_dc = {
        .pin  = GPIO_TFT_DC,
        .mode = GPIO_MODE_OUTPUT,
        .pull = GPIO_PULL_NONE,
    };
    gpio_configure(&gpio_dc);

    gpio_config_t gpio_rst = {
        .pin  = GPIO_TFT_RST,
        .mode = GPIO_MODE_OUTPUT,
        .pull = GPIO_PULL_NONE,
    };
    gpio_configure(&gpio_rst);

    /* ---- Backlight: skip PWM init when BL pin is not connected ---- */
    if (GPIO_TFT_BL == 255) {
        LOG_INFO("Backlight control disabled (GPIO_TFT_BL=255)");
    } else {
        pwm_config_t pwm_cfg = {
            .pin        = GPIO_TFT_BL,
            .frequency  = PWM_FREQ_DEFAULT,
            .duty_cycle = tft_ctx.brightness,
        };
        pwm_init(PWM_CHANNEL_0, &pwm_cfg);
        pwm_enable(PWM_CHANNEL_0);
    }

    /* ---- Hardware + software reset ---- */
    tft_reset();

    tft_write_command(ILI948X_SWRESET);
    delay_ms(150);

    tft_write_command(ILI948X_SLPOUT);
    delay_ms(120);

    /* ---- Controller-specific extended init ---- */
    if (strcmp(TFT_TYPE, "ILI9486") == 0) {
        LOG_INFO("TFT controller profile: ILI9486 (16-bit pixel writes)");
        tft_init_sequence_ili9486();
    } else {
        LOG_INFO("TFT controller profile: %s (generic ILI9488 sequence)", TFT_TYPE);
    }

    /* ---- Interface pixel format: 16-bit RGB565 (0x55) ---- */
    uint8_t colmod_data = 0x55;
    tft_write_command(ILI948X_COLMOD);
    if (tft_write_data(&colmod_data, 1) != HAL_OK) {
        LOG_ERROR("TFT COLMOD write failed");
        return HAL_ERROR;
    }
    delay_ms(10);

    /* ---- Memory access control / initial rotation ---- */
    uint8_t madctl_data;
    switch (tft_ctx.rotation) {
        case 1:  madctl_data = 0x60; break;  /* 90° */
        case 2:  madctl_data = 0xC0; break;  /* 180° */
        case 3:  madctl_data = 0xA0; break;  /* 270° */
        default: madctl_data = 0x00; break;  /* 0° */
    }
    tft_write_command(ILI948X_MADCTL);
    tft_write_data(&madctl_data, 1);

    /* ---- Display on ---- */
    tft_write_command(ILI948X_DISPON);
    delay_ms(100);

    tft_ctx.initialized = 1;
    LOG_INFO("TFT initialized OK — clearing display to black");

    /* Clear display to black */
    tft_clear();

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

    tft_set_address_window(x, y, x + width - 1, y + height - 1);

    tft_write_command(ILI948X_RAMWR);

    uint32_t data_length = (uint32_t)width * height * 2;  /* 2 bytes per RGB565 pixel */
    tft_write_data((const uint8_t *)data, data_length);

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

    tft_set_address_window(x, y, x + width - 1, y + height - 1);

    tft_write_command(ILI948X_RAMWR);

    /* Convert RGB565 color to byte pair */
    uint8_t color_bytes[2] = {
        (color >> 8) & 0xFF,
        color        & 0xFF,
    };

    /* Build a chunk and send in bulk to reduce SPI call overhead */
    enum { TFT_FILL_CHUNK_PIXELS = 256 };
    uint8_t chunk[TFT_FILL_CHUNK_PIXELS * 2];
    for (uint32_t i = 0; i < TFT_FILL_CHUNK_PIXELS; i++) {
        chunk[i * 2]     = color_bytes[0];
        chunk[i * 2 + 1] = color_bytes[1];
    }

    uint32_t remaining = (uint32_t)width * height;
    while (remaining > 0) {
        uint32_t pixels = (remaining > TFT_FILL_CHUNK_PIXELS) ? TFT_FILL_CHUNK_PIXELS : remaining;
        hal_status_t status = tft_write_data(chunk, pixels * 2);
        if (status != HAL_OK) {
            return status;
        }
        remaining -= pixels;
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

    if (GPIO_TFT_BL == 255) {
        return HAL_OK;  /* No backlight pin — brightness control not available */
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

    uint8_t madctl = 0x00;
    switch (rotation) {
        case 0: madctl = 0x00; break;  /* 0° */
        case 1: madctl = 0x60; break;  /* 90° */
        case 2: madctl = 0xC0; break;  /* 180° */
        case 3: madctl = 0xA0; break;  /* 270° */
    }

    tft_write_command(ILI948X_MADCTL);
    tft_write_data(&madctl, 1);

    return HAL_OK;
}

hal_status_t tft_deinit(void)
{
    if (!tft_ctx.initialized) {
        return HAL_OK;
    }

    tft_write_command(ILI948X_DISPOFF);

    if (GPIO_TFT_BL != 255) {
        pwm_disable(PWM_CHANNEL_0);
        pwm_deinit(PWM_CHANNEL_0);
    }

    spi_deinit(SPI_BUS_0);

    tft_ctx.initialized = 0;
    return HAL_OK;
}

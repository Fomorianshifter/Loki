/**
 * TFT Display Driver Implementation for ILI9488
 * Raspberry Pi Zero 2W / Orange Pi Zero 2W - SPI0 Interface
 *
 * NOTE: The ILI9488 over a 4-wire SPI bus only supports 18-bit (RGB666) pixel
 * format (COLMOD = 0x66).  Setting COLMOD = 0x55 (16-bit RGB565) has no effect
 * and results in a blank or garbled display.  Pixel data is therefore always
 * sent as three bytes per pixel (R8, G8, B8 – top 6 bits used by the panel).
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
#define ILI9488_PWCTRL1         0xC0  /* Power control 1 */
#define ILI9488_PWCTRL2         0xC1  /* Power control 2 */
#define ILI9488_VMCTRL1         0xC5  /* VCOM control 1 */
#define ILI9488_PGAMCTRL        0xE0  /* Positive gamma control */
#define ILI9488_NGAMCTRL        0xE1  /* Negative gamma control */

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
    gpio_set(GPIO_TFT_DC, GPIO_LEVEL_LOW);
    return spi_write(SPI_BUS_0, SPI0_CS0, &cmd, 1);
}

/**
 * Send data bytes to TFT
 */
static hal_status_t tft_write_data(const uint8_t *data, uint32_t length)
{
    gpio_set(GPIO_TFT_DC, GPIO_LEVEL_HIGH);
    return spi_write(SPI_BUS_0, SPI0_CS0, data, length);
}

/**
 * Delay in milliseconds
 */
static void delay_ms(uint32_t ms)
{
    usleep(ms * 1000);
}

/**
 * Hardware reset
 */
static void tft_reset(void)
{
    gpio_set(GPIO_TFT_RST, GPIO_LEVEL_LOW);
    delay_ms(10);
    gpio_set(GPIO_TFT_RST, GPIO_LEVEL_HIGH);
    delay_ms(120);
}

/**
 * Set address window for pixel writing
 */
static hal_status_t tft_set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t buf[4];

    tft_write_command(ILI9488_CASET);
    buf[0] = (x0 >> 8) & 0xFF;
    buf[1] = x0 & 0xFF;
    buf[2] = (x1 >> 8) & 0xFF;
    buf[3] = x1 & 0xFF;
    tft_write_data(buf, 4);

    tft_write_command(ILI9488_PASET);
    buf[0] = (y0 >> 8) & 0xFF;
    buf[1] = y0 & 0xFF;
    buf[2] = (y1 >> 8) & 0xFF;
    buf[3] = y1 & 0xFF;
    tft_write_data(buf, 4);

    return HAL_OK;
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t tft_init(void)
{
    if (tft_ctx.initialized) {
        return HAL_OK;
    }

    /* Initialize SPI0 */
    spi_config_t spi_cfg = {
        .frequency = TFT_SPI_FREQ,
        .mode = SPI_MODE_0,
        .bits_per_word = 8,
        .bit_order = SPI_MSB_FIRST,
    };
    if (spi_init(SPI_BUS_0, &spi_cfg) != HAL_OK) {
        return HAL_ERROR;
    }

    /* Configure GPIO control pins */
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

    /* Initialize PWM backlight */
    pwm_config_t pwm_cfg = {
        .pin = GPIO_TFT_BL,
        .frequency = PWM_FREQ_DEFAULT,
        .duty_cycle = tft_ctx.brightness,
    };
    pwm_init(PWM_CHANNEL_0, &pwm_cfg);
    pwm_enable(PWM_CHANNEL_0);

    /* Hardware reset */
    tft_reset();

    /* ---- ILI9488 full init sequence ---- */

    /* Software reset */
    tft_write_command(ILI9488_SWRESET);
    delay_ms(120);

    /* Sleep out */
    tft_write_command(ILI9488_SLPOUT);
    delay_ms(120);

    /* Power control 1: VRH = 4.60V */
    tft_write_command(ILI9488_PWCTRL1);
    { uint8_t d[] = {0x17, 0x15}; tft_write_data(d, 2); }

    /* Power control 2: SAP */
    tft_write_command(ILI9488_PWCTRL2);
    { uint8_t d[] = {0x41}; tft_write_data(d, 1); }

    /* VCOM control */
    tft_write_command(ILI9488_VMCTRL1);
    { uint8_t d[] = {0x00, 0x12, 0x80}; tft_write_data(d, 3); }

    /* Memory access control: default orientation (landscape) */
    tft_write_command(ILI9488_MADCTL);
    { uint8_t d[] = {0x48}; tft_write_data(d, 1); }

    /* Pixel format: 18bpp RGB666 — only valid SPI mode for ILI9488 */
    tft_write_command(ILI9488_COLMOD);
    { uint8_t d[] = {0x66}; tft_write_data(d, 1); }

    /* Positive gamma */
    tft_write_command(ILI9488_PGAMCTRL);
    { uint8_t d[] = {0x00,0x03,0x09,0x08,0x16,0x0A,0x3F,0x78,
                     0x4C,0x09,0x0A,0x08,0x16,0x1A,0x0F};
      tft_write_data(d, 15); }

    /* Negative gamma */
    tft_write_command(ILI9488_NGAMCTRL);
    { uint8_t d[] = {0x00,0x16,0x19,0x03,0x0F,0x05,0x32,0x45,
                     0x46,0x04,0x0E,0x0D,0x35,0x37,0x0F};
      tft_write_data(d, 15); }

    /* Display on */
    tft_write_command(ILI9488_DISPON);
    delay_ms(100);

    /* Clear to black */
    tft_ctx.initialized = 1;  /* must be set before tft_clear */
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
    tft_write_command(ILI9488_RAMWR);

    /* Convert RGB565 pixel array to 18bpp RGB666 (3 bytes/pixel) */
    uint32_t pixel_count = (uint32_t)width * height;
    enum { CHUNK = 64 };
    uint8_t buf[CHUNK * 3];
    uint32_t i = 0;

    while (i < pixel_count) {
        uint32_t n = pixel_count - i;
        if (n > CHUNK) n = CHUNK;
        for (uint32_t j = 0; j < n; j++) {
            color_t px = data[i + j];
            buf[j * 3 + 0] = (uint8_t)((px >> 8) & 0xF8);  /* R */
            buf[j * 3 + 1] = (uint8_t)((px >> 3) & 0xFC);  /* G */
            buf[j * 3 + 2] = (uint8_t)((px << 3) & 0xF8);  /* B */
        }
        hal_status_t st = tft_write_data(buf, n * 3);
        if (st != HAL_OK) return st;
        i += n;
    }

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
    tft_write_command(ILI9488_RAMWR);

    /* Pre-fill a chunk buffer with the 18bpp encoding of the colour */
    uint8_t r = (uint8_t)((color >> 8) & 0xF8);
    uint8_t g = (uint8_t)((color >> 3) & 0xFC);
    uint8_t b = (uint8_t)((color << 3) & 0xF8);

    enum { TFT_FILL_CHUNK_PIXELS = 256 };
    uint8_t chunk[TFT_FILL_CHUNK_PIXELS * 3];
    for (uint32_t i = 0; i < TFT_FILL_CHUNK_PIXELS; i++) {
        chunk[i * 3 + 0] = r;
        chunk[i * 3 + 1] = g;
        chunk[i * 3 + 2] = b;
    }

    uint32_t remaining = (uint32_t)width * height;
    while (remaining > 0) {
        uint32_t pixels = (remaining > TFT_FILL_CHUNK_PIXELS) ? TFT_FILL_CHUNK_PIXELS : remaining;
        hal_status_t st = tft_write_data(chunk, pixels * 3);
        if (st != HAL_OK) return st;
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
    tft_write_command(ILI9488_MADCTL);

    uint8_t madctl = 0x00;
    switch (rotation) {
        case 0: madctl = 0x48; break;  /* 0°   – landscape default */
        case 1: madctl = 0x28; break;  /* 90°  */
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

    tft_write_command(ILI9488_DISPOFF);
    pwm_disable(PWM_CHANNEL_0);
    pwm_deinit(PWM_CHANNEL_0);
    spi_deinit(SPI_BUS_0);

    tft_ctx.initialized = 0;
    return HAL_OK;
}

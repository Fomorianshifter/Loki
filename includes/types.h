#ifndef TYPES_H
#define TYPES_H

/**
 * Common types and status codes for Loki system
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ===== STATUS CODES ===== */
typedef enum {
    HAL_OK           = 0,
    HAL_ERROR        = -1,
    HAL_TIMEOUT      = -2,
    HAL_NOT_READY    = -3,
    HAL_BUSY         = -4,
    HAL_INVALID_PARAM = -5,
    HAL_NOT_SUPPORTED = -6,
} hal_status_t;

/* ===== SPI CONFIGURATION ===== */
typedef enum {
    SPI_MODE_0 = 0,  /* CPOL=0, CPHA=0 */
    SPI_MODE_1 = 1,  /* CPOL=0, CPHA=1 */
    SPI_MODE_2 = 2,  /* CPOL=1, CPHA=0 */
    SPI_MODE_3 = 3,  /* CPOL=1, CPHA=1 */
} spi_mode_t;

typedef enum {
    SPI_LSB_FIRST = 0,
    SPI_MSB_FIRST = 1,
} spi_bit_order_t;

typedef struct {
    uint32_t frequency;      /* SPI frequency in Hz */
    uint8_t mode;            /* SPI mode (0-3) */
    uint8_t bits_per_word;   /* Usually 8 */
    spi_bit_order_t bit_order;
} spi_config_t;

/* ===== I2C CONFIGURATION ===== */
typedef enum {
    I2C_STANDARD_MODE   = 100000,   /* 100 kHz */
    I2C_FAST_MODE       = 400000,   /* 400 kHz */
    I2C_FAST_PLUS_MODE  = 1000000,  /* 1 MHz */
} i2c_speed_t;

typedef struct {
    uint32_t frequency;      /* I2C frequency in Hz */
    uint8_t address_bits;    /* 7 or 10 bit addressing */
} i2c_config_t;

/* ===== UART CONFIGURATION ===== */
typedef enum {
    UART_DATA_BITS_5 = 5,
    UART_DATA_BITS_6 = 6,
    UART_DATA_BITS_7 = 7,
    UART_DATA_BITS_8 = 8,
} uart_data_bits_t;

typedef enum {
    UART_STOP_BITS_1 = 1,
    UART_STOP_BITS_2 = 2,
} uart_stop_bits_t;

typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_ODD  = 1,
    UART_PARITY_EVEN = 2,
} uart_parity_t;

typedef struct {
    uint32_t baud_rate;
    uart_data_bits_t data_bits;
    uart_stop_bits_t stop_bits;
    uart_parity_t parity;
} uart_config_t;

/* ===== GPIO CONFIGURATION ===== */
typedef enum {
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT = 1,
    GPIO_MODE_ALTERNATE = 2,
} gpio_mode_t;

typedef enum {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP = 1,
    GPIO_PULL_DOWN = 2,
} gpio_pull_t;

typedef enum {
    GPIO_LEVEL_LOW = 0,
    GPIO_LEVEL_HIGH = 1,
} gpio_level_t;

typedef struct {
    uint32_t pin;
    gpio_mode_t mode;
    gpio_pull_t pull;
} gpio_config_t;

/* ===== PWM CONFIGURATION ===== */
typedef struct {
    uint32_t pin;
    uint32_t frequency;  /* Hz */
    uint8_t duty_cycle;  /* 0-100 */
} pwm_config_t;

/* ===== MEMORY OPERATIONS ===== */
typedef struct {
    uint32_t address;
    uint32_t length;
    uint8_t *data;
} memory_operation_t;

/* ===== COLOR DEFINITIONS ===== */
typedef uint16_t color_t;  /* 16-bit RGB565 color */

#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))
#define COLOR_BLACK   RGB565(0, 0, 0)
#define COLOR_WHITE   RGB565(255, 255, 255)
#define COLOR_RED     RGB565(255, 0, 0)
#define COLOR_GREEN   RGB565(0, 255, 0)
#define COLOR_BLUE    RGB565(0, 0, 255)

#endif /* TYPES_H */

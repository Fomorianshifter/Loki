/**
 * UART Hardware Abstraction Layer Implementation
 * Orange Pi Zero 2W
 */

#include "uart.h"
#include <stdio.h>
#include <string.h>

/* ===== UART DEVICE CONTEXT ===== */
typedef struct {
    uart_port_t port;
    int device_handle;
    uart_config_t config;
    uart_rx_callback_t rx_callback;
    uint8_t initialized;
} uart_context_t;

/* UART context */
static uart_context_t uart_context_1 = {
    .port = UART_PORT_1,
    .rx_callback = NULL,
    .initialized = 0,
};

/* ===== RECEIVE BUFFER RINGBUFFER ===== */
#define UART_RX_BUFFER_SIZE 256

typedef struct {
    uint8_t buffer[UART_RX_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
} ring_buffer_t;

static ring_buffer_t uart1_rx_buffer = {0};

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t uart_init(uart_port_t port, const uart_config_t *config)
{
    if (config == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (port != UART_PORT_1) {
        return HAL_INVALID_PARAM;
    }

    uart_context_t *ctx = &uart_context_1;

    if (ctx->initialized) {
        return HAL_OK;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(uart_config_t));

    /* Open UART device at /dev/ttyS1 or /dev/ttyUSB0 */
    /* Set baud rate, data bits, parity, stop bits */
    /* Configure flow control (if needed) */
    /* Enable interrupts for receive */

    /* Initialize RX ring buffer */
    uart1_rx_buffer.head = 0;
    uart1_rx_buffer.tail = 0;

    ctx->initialized = 1;
    return HAL_OK;
}

hal_status_t uart_send(uart_port_t port, const uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0) {
        return HAL_INVALID_PARAM;
    }

    if (port != UART_PORT_1) {
        return HAL_INVALID_PARAM;
    }

    uart_context_t *ctx = &uart_context_1;

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    /* Send data bytes */
    /* Wait for UART transmit to complete */

    return HAL_OK;
}

hal_status_t uart_send_byte(uart_port_t port, uint8_t byte)
{
    uint8_t data[1] = {byte};
    return uart_send(port, data, 1);
}

hal_status_t uart_receive(uart_port_t port, uint8_t *data, uint32_t length, uint32_t timeout_ms)
{
    if (data == NULL || length == 0) {
        return HAL_INVALID_PARAM;
    }

    if (port != UART_PORT_1) {
        return HAL_INVALID_PARAM;
    }

    uart_context_t *ctx = &uart_context_1;

    if (!ctx->initialized) {
        return HAL_NOT_READY;
    }

    /* Wait for data in RX buffer with timeout */
    /* Copy data from ring buffer to user buffer */

    return HAL_OK;
}

hal_status_t uart_receive_byte(uart_port_t port, uint8_t *byte, uint32_t timeout_ms)
{
    if (byte == NULL) {
        return HAL_INVALID_PARAM;
    }

    return uart_receive(port, byte, 1, timeout_ms);
}

uint32_t uart_available(uart_port_t port)
{
    if (port != UART_PORT_1) {
        return 0;
    }

    ring_buffer_t *rb = &uart1_rx_buffer;
    if (rb->head >= rb->tail) {
        return (rb->head - rb->tail);
    } else {
        return (UART_RX_BUFFER_SIZE - rb->tail + rb->head);
    }
}

hal_status_t uart_set_rx_callback(uart_port_t port, uart_rx_callback_t callback)
{
    if (port != UART_PORT_1) {
        return HAL_INVALID_PARAM;
    }

    uart_context_t *ctx = &uart_context_1;
    ctx->rx_callback = callback;
    return HAL_OK;
}

hal_status_t uart_flush(uart_port_t port)
{
    if (port != UART_PORT_1) {
        return HAL_INVALID_PARAM;
    }

    uart1_rx_buffer.head = 0;
    uart1_rx_buffer.tail = 0;
    return HAL_OK;
}

hal_status_t uart_deinit(uart_port_t port)
{
    if (port != UART_PORT_1) {
        return HAL_INVALID_PARAM;
    }

    uart_context_t *ctx = &uart_context_1;

    if (!ctx->initialized) {
        return HAL_OK;
    }

    /* Disable UART interrupts */
    /* Close UART device */
    /* if (ctx->device_handle >= 0) {
        close(ctx->device_handle);
    } */

    ctx->initialized = 0;
    return HAL_OK;
}

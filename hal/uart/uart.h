#ifndef UART_H
#define UART_H

/**
 * UART Hardware Abstraction Layer for Orange Pi Zero 2W
 * Supports UART1 for Flipper Zero communication
 */

#include "../../includes/types.h"

/* ===== UART DEFINITIONS ===== */
typedef enum {
    UART_PORT_1 = 1,  /* Flipper Zero communication */
    UART_PORT_COUNT = 1,
} uart_port_t;

/* ===== CALLBACKS ===== */
typedef void (*uart_rx_callback_t)(uint8_t byte);
typedef void (*uart_tx_complete_callback_t)(void);

/* ===== PUBLIC API ===== */

/**
 * Initialize UART port
 * @param[in] port UART port number
 * @param[in] config UART configuration
 * @return HAL_OK on success
 */
hal_status_t uart_init(uart_port_t port, const uart_config_t *config);

/**
 * Send data via UART (blocking)
 * @param[in] port UART port number
 * @param[in] data Pointer to data buffer
 * @param[in] length Number of bytes to send
 * @return HAL_OK on success
 */
hal_status_t uart_send(uart_port_t port, const uint8_t *data, uint32_t length);

/**
 * Send single byte via UART (blocking)
 * @param[in] port UART port number
 * @param[in] byte Byte to send
 * @return HAL_OK on success
 */
hal_status_t uart_send_byte(uart_port_t port, uint8_t byte);

/**
 * Receive data from UART (blocking with timeout)
 * @param[in] port UART port number
 * @param[out] data Pointer to receive buffer
 * @param[in] length Number of bytes to receive
 * @param[in] timeout_ms Timeout in milliseconds (0 = wait forever)
 * @return HAL_OK on success, HAL_TIMEOUT if no data received
 */
hal_status_t uart_receive(uart_port_t port, uint8_t *data, uint32_t length, uint32_t timeout_ms);

/**
 * Receive single byte from UART (blocking with timeout)
 * @param[in] port UART port number
 * @param[out] byte Pointer to receive byte
 * @param[in] timeout_ms Timeout in milliseconds
 * @return HAL_OK on success, HAL_TIMEOUT if no data received
 */
hal_status_t uart_receive_byte(uart_port_t port, uint8_t *byte, uint32_t timeout_ms);

/**
 * Get number of bytes available in receive buffer
 * @param[in] port UART port number
 * @return Number of bytes available
 */
uint32_t uart_available(uart_port_t port);

/**
 * Set receive callback for non-blocking operation
 * @param[in] port UART port number
 * @param[in] callback Callback function pointer
 * @return HAL_OK on success
 */
hal_status_t uart_set_rx_callback(uart_port_t port, uart_rx_callback_t callback);

/**
 * Flush UART receive buffer
 * @param[in] port UART port number
 * @return HAL_OK on success
 */
hal_status_t uart_flush(uart_port_t port);

/**
 * Deinitialize UART port
 * @param[in] port UART port number
 * @return HAL_OK on success
 */
hal_status_t uart_deinit(uart_port_t port);

#endif /* UART_H */

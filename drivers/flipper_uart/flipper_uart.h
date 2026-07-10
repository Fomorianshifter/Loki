#ifndef FLIPPER_UART_H
#define FLIPPER_UART_H

/**
 * Flipper Zero UART Communication Driver
 * Bidirectional serial protocol over UART1
 */

#include "../../includes/types.h"
#include "../../config/board_config.h"

/* ===== FLIPPER MESSAGE PROTOCOL ===== */

#define FLIPPER_MSG_HEADER_SIZE     2
#define FLIPPER_MSG_CMD_OFFSET      0
#define FLIPPER_MSG_LEN_OFFSET      1
#define FLIPPER_MSG_MAX_PAYLOAD     256

typedef enum {
    FLIPPER_CMD_ACK           = 0x00,
    FLIPPER_CMD_NACK          = 0x01,
    FLIPPER_CMD_HELLO         = 0x02,
    FLIPPER_CMD_GOODBYE       = 0x03,
    FLIPPER_CMD_REQUEST_STATE = 0x10,
    FLIPPER_CMD_STATE_UPDATE  = 0x11,
    FLIPPER_CMD_REQUEST_DATA  = 0x20,
    FLIPPER_CMD_SEND_DATA     = 0x21,
    FLIPPER_CMD_CONTROL       = 0x30,
    FLIPPER_CMD_DEBUG         = 0xF0,
} flipper_cmd_t;

typedef struct {
    uint8_t cmd;
    uint16_t length;
    uint8_t *payload;
} flipper_message_t;

/* ===== PUBLIC API ===== */

/**
 * Initialize Flipper UART communication
 * @return HAL_OK on success
 */
hal_status_t flipper_uart_init(void);

/**
 * Send message to Flipper
 * @param[in] message Pointer to message structure
 * @return HAL_OK on success
 */
hal_status_t flipper_send_message(const flipper_message_t *message);

/**
 * Receive message from Flipper (blocking)
 * @param[out] message Pointer to receive message
 * @param[in] timeout_ms Timeout in milliseconds
 * @return HAL_OK on success
 */
hal_status_t flipper_receive_message(flipper_message_t *message, uint32_t timeout_ms);

/**
 * Check if data available from Flipper
 * @return Number of bytes available, 0 if none
 */
uint32_t flipper_available(void);

/**
 * Deinitialize Flipper UART
 * @return HAL_OK on success
 */
hal_status_t flipper_uart_deinit(void);

#endif /* FLIPPER_UART_H */

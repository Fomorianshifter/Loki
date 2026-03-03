/**
 * Flipper Zero UART Communication Driver Implementation
 * Orange Pi Zero 2W - UART1 Interface
 */

#include "flipper_uart.h"
#include "../../hal/uart/uart.h"
#include "../../config/pinout.h"
#include <string.h>

/* ===== FLIPPER UART STATE ===== */
typedef struct {
    uint8_t initialized;
    uint8_t connected;
} flipper_uart_context_t;

static flipper_uart_context_t flipper_ctx = {
    .initialized = 0,
    .connected = 0,
};

/* ===== LOCAL HELPER FUNCTIONS ===== */

/**
 * Calculate simple checksum for message
 */
static uint8_t flipper_checksum(const uint8_t *data, uint16_t length)
{
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t flipper_uart_init(void)
{
    if (flipper_ctx.initialized) {
        return HAL_OK;
    }

    /* Initialize UART1 for Flipper */
    uart_config_t uart_cfg = {
        .baud_rate = UART1_BAUD_RATE,
        .data_bits = UART_DATA_BITS_8,
        .stop_bits = UART_STOP_BITS_1,
        .parity = UART_PARITY_NONE,
    };
    
    if (uart_init(UART_PORT_1, &uart_cfg) != HAL_OK) {
        return HAL_ERROR;
    }

    flipper_ctx.initialized = 1;
    
    /* Handshake: send HELLO message */
    flipper_message_t hello = {
        .cmd = FLIPPER_CMD_HELLO,
        .length = 0,
        .payload = NULL,
    };
    flipper_send_message(&hello);

    flipper_ctx.connected = 1;
    return HAL_OK;
}

hal_status_t flipper_send_message(const flipper_message_t *message)
{
    if (message == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (!flipper_ctx.initialized) {
        return HAL_NOT_READY;
    }

    /* Build packet: [CMD, LEN_HI, LEN_LO, PAYLOAD..., CHECKSUM] */
    uint8_t packet[FLIPPER_MSG_MAX_PAYLOAD + 4];
    
    packet[0] = message->cmd;
    packet[1] = (message->length >> 8) & 0xFF;
    packet[2] = message->length & 0xFF;

    if (message->payload != NULL && message->length > 0) {
        memcpy(&packet[3], message->payload, message->length);
    }

    uint16_t packet_length = 3 + message->length;
    packet[packet_length] = flipper_checksum(packet, packet_length);
    packet_length++;

    /* Send packet */
    return uart_send(UART_PORT_1, packet, packet_length);
}

hal_status_t flipper_receive_message(flipper_message_t *message, uint32_t timeout_ms)
{
    if (message == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (!flipper_ctx.initialized) {
        return HAL_NOT_READY;
    }

    /* Read header: [CMD, LEN_HI, LEN_LO] */
    uint8_t header[3];
    hal_status_t status = uart_receive(UART_PORT_1, header, 3, timeout_ms);
    if (status != HAL_OK) {
        return status;
    }

    message->cmd = header[0];
    uint16_t payload_length = ((uint16_t)header[1] << 8) | header[2];

    if (payload_length > 0 && payload_length <= FLIPPER_MSG_MAX_PAYLOAD) {
        /* Read payload + checksum */
        uint8_t payload_and_sum[FLIPPER_MSG_MAX_PAYLOAD + 1];
        status = uart_receive(UART_PORT_1, payload_and_sum, payload_length + 1, timeout_ms);
        if (status != HAL_OK) {
            return status;
        }

        /* Verify checksum */
        uint8_t expected_checksum = flipper_checksum(header, 3);
        expected_checksum ^= flipper_checksum(payload_and_sum, payload_length);

        if (expected_checksum != payload_and_sum[payload_length]) {
            return HAL_ERROR;  /* Checksum mismatch */
        }

        message->length = payload_length;
        message->payload = malloc(payload_length);
        if (message->payload == NULL) {
            return HAL_ERROR;
        }
        memcpy(message->payload, payload_and_sum, payload_length);
    } else if (payload_length == 0) {
        /* No payload, but read and verify checksum */
        uint8_t checksum;
        status = uart_receive_byte(UART_PORT_1, &checksum, timeout_ms);
        if (status != HAL_OK) {
            return status;
        }

        uint8_t expected_checksum = flipper_checksum(header, 3);
        if (expected_checksum != checksum) {
            return HAL_ERROR;  /* Checksum mismatch */
        }

        message->length = 0;
        message->payload = NULL;
    } else {
        return HAL_INVALID_PARAM;  /* Payload too large */
    }

    return HAL_OK;
}

uint32_t flipper_available(void)
{
    if (!flipper_ctx.initialized) {
        return 0;
    }

    return uart_available(UART_PORT_1);
}

hal_status_t flipper_uart_deinit(void)
{
    if (!flipper_ctx.initialized) {
        return HAL_OK;
    }

    /* Send GOODBYE message */
    flipper_message_t goodbye = {
        .cmd = FLIPPER_CMD_GOODBYE,
        .length = 0,
        .payload = NULL,
    };
    flipper_send_message(&goodbye);

    uart_deinit(UART_PORT_1);
    flipper_ctx.initialized = 0;
    flipper_ctx.connected = 0;
    return HAL_OK;
}

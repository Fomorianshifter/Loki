#ifndef BOARD_PROFILE_H
#define BOARD_PROFILE_H

/**
 * @file board_profile.h
 * @brief Linux SBC board profile metadata and safe hardware defaults.
 */

#include <stdint.h>

#define LOKI_DEFAULT_BOARD_PROFILE_ID "raspberry_pi_zero_w"
#define LOKI_BOARD_PROFILE_CHOICES "raspberry_pi_zero_w/orange_pi_zero_2w/generic_linux_sbc"

typedef struct {
    const char *id;
    const char *display_name;
    const char *model;

    uint16_t tft_width;
    uint16_t tft_height;
    uint32_t tft_spi_freq_hz;
    uint32_t sd_spi_freq_hz;
    uint32_t flash_spi_freq_hz;
    uint32_t eeprom_i2c_freq_hz;
    uint32_t uart_baud_rate;

    float power_input_voltage;
    float power_logic_voltage;
} loki_board_profile_t;

const loki_board_profile_t *loki_board_profile_default(void);
const loki_board_profile_t *loki_board_profile_get(const char *profile_id);
int loki_board_profile_is_supported(const char *profile_id);

#endif /* BOARD_PROFILE_H */

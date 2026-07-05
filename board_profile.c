#include "board_profile.h"

#include <string.h>

static const loki_board_profile_t BOARD_PROFILES[] = {
    {
        .id = "raspberry_pi_zero_w",
        .display_name = "Raspberry Pi Zero W",
        .model = "RPI_ZERO_W",
        .tft_width = 480,
        .tft_height = 320,
        .tft_spi_freq_hz = 32000000,
        .sd_spi_freq_hz = 25000000,
        .flash_spi_freq_hz = 20000000,
        .eeprom_i2c_freq_hz = 100000,
        .uart_baud_rate = 115200,
        .power_input_voltage = 5.0f,
        .power_logic_voltage = 3.3f,
    },
    {
        .id = "orange_pi_zero_2w",
        .display_name = "Orange Pi Zero 2W",
        .model = "OPI_ZERO_2W",
        .tft_width = 480,
        .tft_height = 320,
        .tft_spi_freq_hz = 40000000,
        .sd_spi_freq_hz = 25000000,
        .flash_spi_freq_hz = 20000000,
        .eeprom_i2c_freq_hz = 100000,
        .uart_baud_rate = 115200,
        .power_input_voltage = 5.0f,
        .power_logic_voltage = 3.3f,
    },
    {
        .id = "generic_linux_sbc",
        .display_name = "Generic Linux SBC",
        .model = "GENERIC_LINUX_SBC",
        .tft_width = 480,
        .tft_height = 320,
        .tft_spi_freq_hz = 24000000,
        .sd_spi_freq_hz = 16000000,
        .flash_spi_freq_hz = 12000000,
        .eeprom_i2c_freq_hz = 100000,
        .uart_baud_rate = 115200,
        .power_input_voltage = 5.0f,
        .power_logic_voltage = 3.3f,
    },
};

static const unsigned int BOARD_PROFILE_COUNT = sizeof(BOARD_PROFILES) / sizeof(BOARD_PROFILES[0]);

const loki_board_profile_t *loki_board_profile_default(void)
{
    return &BOARD_PROFILES[0];
}

const loki_board_profile_t *loki_board_profile_get(const char *profile_id)
{
    unsigned int i;

    if (profile_id == NULL || profile_id[0] == '\0') {
        return loki_board_profile_default();
    }

    for (i = 0; i < BOARD_PROFILE_COUNT; i++) {
        if (strcmp(BOARD_PROFILES[i].id, profile_id) == 0) {
            return &BOARD_PROFILES[i];
        }
    }

    return NULL;
}

int loki_board_profile_is_supported(const char *profile_id)
{
    return loki_board_profile_get(profile_id) != NULL;
}
